/* Copyright (C) 2003 Massachusetts Institute of Technology
%
%  This program is free software; you can redistribute it and/or modify
%  it under the terms of the GNU General Public License as published by
%  the Free Software Foundation; either version 2, or (at your option)
%  any later version.
%
%  This program is distributed in the hope that it will be useful,
%  but WITHOUT ANY WARRANTY; without even the implied warranty of
%  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%  GNU General Public License for more details.
%
%  You should have received a copy of the GNU General Public License
%  along with this program; if not, write to the Free Software Foundation,
%  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex>

#include "dactyl.h"
#include "dactyl_internals.h"

void fields::step_right() {
  phase_material();

  step_h_right();
  step_h_source();
  step_boundaries(H_stuff);

  prepare_step_polarization_energy();
  half_step_polarization_energy();
  step_e_right();
  step_e_source();
  step_e_polarization();
  step_boundaries(E_stuff);
  half_step_polarization_energy();

  update_polarization_saturation();
  step_polarization_itself();

  t += 1;
}

void fields::step() {
  phase_material();

  step_h();
  step_h_source();
  step_boundaries(H_stuff);

  prepare_step_polarization_energy();
  half_step_polarization_energy();
  step_e();
  step_e_source();
  step_e_polarization();
  step_boundaries(E_stuff);
  half_step_polarization_energy();

  update_polarization_saturation();
  step_polarization_itself();

  t += 1;
}

void fields::phase_material() {
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      chunks[i]->phase_material(phasein_time);
  phasein_time--;
}

void fields_chunk::phase_material(int phasein_time) {
  if (new_ma && phasein_time > 0) {
    // First multiply the electric field by epsilon...
    DOCMP {
      for (int c=0;c<10;c++)
        if (v.has_field((component)c) && is_electric((component)c))
          for (int i=0;i<v.ntot();i++)
            f[c][cmp][i] /= ma->inveps[c][i];
      for (int c=0;c<10;c++)
        if (v.has_field((component)c) && is_electric((component)c))
          for (int i=0;i<v.ntot();i++)
            f_pml[c][cmp][i] /= ma->inveps[c][i];
    }
    // Then change epsilon...
    ma->mix_with(new_ma, 1.0/phasein_time);
    // Finally divide the electric field by the new epsilon...
    DOCMP {
      for (int c=0;c<10;c++)
        if (v.has_field((component)c) && is_electric((component)c))
          for (int i=0;i<v.ntot();i++)
            f[c][cmp][i] *= ma->inveps[c][i];
      for (int c=0;c<10;c++)
        if (v.has_field((component)c) && is_electric((component)c))
          for (int i=0;i<v.ntot();i++)
            f_pml[c][cmp][i] *= ma->inveps[c][i];
    }
  }
}

inline double it(int cmp, double *(f[2]), int ind) { return (1-2*cmp)*f[1-cmp][ind]; }

inline int rstart_0(const volume &v, int m) {
  return (int) max(0.0, m - (int)(v.origin.r()*v.a+0.5) - 1.0);
}
inline int rstart_1(const volume &v, int m) {
  return (int) max(1.0, (double)m - (int)(v.origin.r()*v.a+0.5));
}

void fields::step_h_right() {
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      chunks[i]->step_h_right();
}

void fields_chunk::step_h_right() {
  const volume v = this->v;
  if (v.dim == d1) {
    DOCMP {
      for (int z=0;z<v.nz();z++)
        f[Hy][cmp][z] = f[Ex][cmp][z];
    }
  } else if (v.dim == dcyl) {
    DOCMP {
      for (int r=rstart_1(v,m);r<=v.nr();r++) {
        const int ir = r*(v.nz()+1);
        for (int z=0;z<v.nz();z++) f[Hr][cmp][z+ir] = f[Ep][cmp][z+ir];
      }
      for (int r=rstart_0(v,m);r<v.nr();r++) {
        const int ir = r*(v.nz()+1);
        const int irp1 = (r+1)*(v.nz()+1);
        for (int z=0;z<v.nz();z++) f[Hp][cmp][z+ir] = f[Ez][cmp][z+irp1];
      }
      for (int r=rstart_0(v,m);r<v.nr();r++) {
        const int ir = r*(v.nz()+1);
        for (int z=1;z<=v.nz();z++) f[Hz][cmp][z+ir] = f[Er][cmp][z+ir];
      }
    }
  }
}

void fields::step_e_right() {
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      chunks[i]->step_e_right();
}

void fields_chunk::step_e_right() {
  const volume v = this->v;
  if (v.dim == d1) {
    DOCMP {
      for (int z=0;z<v.nz();z++)
        f[Hy][cmp][z] = f[Ex][cmp][z];
    }
  } else if (v.dim == dcyl) {
    DOCMP {
      for (int r=rstart_1(v,m);r<=v.nr();r++) {
        const int ir = r*(v.nz()+1);
        const int irm1 = (r-1)*(v.nz()+1);
        for (int z=1;z<=v.nz();z++) f[Ep][cmp][z+ir] = f[Hz][cmp][z+irm1];
      }
      for (int r=rstart_0(v,m);r<v.nr();r++) {
        const int ir = r*(v.nz()+1);
        for (int z=1;z<=v.nz();z++) f[Er][cmp][z+ir] = f[Hp][cmp][z+ir-1];
      }
      for (int r=rstart_1(v,m);r<=v.nr();r++) {
        const int ir = r*(v.nz()+1);
        for (int z=0;z<v.nz();z++) f[Ez][cmp][z+ir] = f[Hr][cmp][z+ir];
      }
    }
  }
}

void fields::step_h() {
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      chunks[i]->step_h();
}

void fields_chunk::step_h() {
  const volume v = this->v;
  if (v.dim == d1) {
    DOCMP {
      if (ma->Cmain[Hy])
        for (int z=0;z<v.nz();z++) {
          const double C = ma->Cmain[Hy][z];
          const double ooop_C = 1.0/(1.0+0.5*C);
          f[Hy][cmp][z] += ooop_C*(-c*(f[Ex][cmp][z+1] - f[Ex][cmp][z])
                                   - C*f[Hy][cmp][z]);
        }
      else
        for (int z=0;z<v.nz();z++)
          f[Hy][cmp][z] += -c*(f[Ex][cmp][z+1] - f[Ex][cmp][z]);
    }
  } else if (v.dim == dcyl) {
    DOCMP {
      // Propogate Hr
      if (ma->Cother[Hr])
        for (int r=rstart_1(v,m);r<=v.nr();r++) {
            double oor = 1.0/((int)(v.origin.r()*v.a+0.5) + r);
            double mor = m*oor;
            const int ir = r*(v.nz()+1);
            for (int z=0;z<v.nz();z++) {
              const double Czhr = ma->Cother[Hr][z+ir];
              const double ooop_Czhr = 1.0/(1.0+0.5*Czhr);
              double dhrp = c*(-it(cmp,f[Ez],z+ir)*mor);
              double hrz = f[Hr][cmp][z+ir] - f_pml[Hr][cmp][z+ir];
              f_pml[Hr][cmp][z+ir] += dhrp;
              f[Hr][cmp][z+ir] += dhrp +
                ooop_Czhr*(c*(f[Ep][cmp][z+ir+1]-f[Ep][cmp][z+ir]) - Czhr*hrz);
            }
          }
      else
        for (int r=rstart_1(v,m);r<=v.nr();r++) {
            double oor = 1.0/((int)(v.origin.r()*v.a + 0.5) + r);
            double mor = m*oor;
            const int ir = r*(v.nz()+1);
            for (int z=0;z<v.nz();z++)
              f[Hr][cmp][z+ir] += c*
                ((f[Ep][cmp][z+ir+1]-f[Ep][cmp][z+ir]) - it(cmp,f[Ez],z+ir)*mor);
          }
      // Propogate Hp
      if (ma->Cmain[Hp] || ma->Cother[Hp])
        for (int r=rstart_0(v,m);r<v.nr();r++) {
            double oorph = 1.0/((int)(v.origin.r()*v.a+0.5) + r+0.5);
            double morph = m*oorph;
            const int ir = r*(v.nz()+1);
            const int irp1 = (r+1)*(v.nz()+1);
            for (int z=0;z<v.nz();z++) {
              const double Czhp = (ma->Cmain[Hp])?ma->Cmain[Hp][z+ir]:0;
              const double Crhp = (ma->Cother[Hp])?ma->Cother[Hp][z+ir]:0;
              const double ooop_Czhp = 1.0/(1.0+0.5*Czhp);
              const double ooop_Crhp = 1.0/(1.0+0.5*Crhp);
              const double dhpz = ooop_Czhp*(-c*(f[Er][cmp][z+ir+1]-f[Er][cmp][z+ir])
                                             - Czhp*f_pml[Hp][cmp][z+ir]);
              const double hpr = f[Hp][cmp][z+ir]-f_pml[Hp][cmp][z+ir];
              f_pml[Hp][cmp][z+ir] += dhpz;
              f[Hp][cmp][z+ir] += dhpz +
                ooop_Czhp*(c*(f[Ez][cmp][z+irp1]-f[Ez][cmp][z+ir]) - Crhp*hpr);
            }
          }
      else 
        for (int r=rstart_0(v,m);r<v.nr();r++) {
            double oorph = 1.0/((int)(v.origin.r()*v.a+0.5) + r+0.5);
            double morph = m*oorph;
            const int ir = r*(v.nz()+1);
            const int irp1 = (r+1)*(v.nz()+1);
            for (int z=0;z<v.nz();z++)
              f[Hp][cmp][z+ir] += c*
                ((f[Ez][cmp][z+irp1]-f[Ez][cmp][z+ir])
                 - (f[Er][cmp][z+ir+1]-f[Er][cmp][z+ir]));
          }
      // Propogate Hz
      if (ma->Cother[Hz])
        for (int r=rstart_0(v,m);r<v.nr();r++) {
          double oorph = 1.0/((int)(v.origin.r()*v.a+0.5) + r+0.5);
          double morph = m*oorph;
          const int ir = r*(v.nz()+1);
          const int irp1 = (r+1)*(v.nz()+1);
          for (int z=1;z<=v.nz();z++) {
            const double Crhz = ma->Cother[Hz][z+ir];
            const double ooop_Crhz = 1.0/(1.0+0.5*Crhz);
            const double dhzr =
              ooop_Crhz*(-c*(f[Ep][cmp][z+irp1]*((int)(v.origin.r()*v.a+0.5) + r+1.)-
                             f[Ep][cmp][z+ir]*((int)(v.origin.r()*v.a+0.5) + r))*oorph
                         - Crhz*f_pml[Hz][cmp][z+ir]);
            const double hzp = f[Hz][cmp][z+ir] - f_pml[Hz][cmp][z+ir];
            f_pml[Hz][cmp][z+ir] += dhzr;
            f[Hz][cmp][z+ir] += dhzr + c*(it(cmp,f[Er],z+ir)*morph);
          }
        }
      else
        for (int r=rstart_0(v,m);r<v.nr();r++) {
          double oorph = 1.0/((int)(v.origin.r()*v.a+0.5) + r+0.5);
          double morph = m*oorph;
          const int ir = r*(v.nz()+1);
          const int irp1 = (r+1)*(v.nz()+1);
          for (int z=1;z<=v.nz();z++)
            f[Hz][cmp][z+ir] += c*
              (it(cmp,f[Er],z+ir)*morph
               - (f[Ep][cmp][z+irp1]*((int)(v.origin.r()*v.a+0.5) + r+1.)-
                  f[Ep][cmp][z+ir]*((int)(v.origin.r()*v.a+0.5) + r))*oorph);
        }
      // Deal with annoying r==0 boundary conditions...
      if (m == 0) {
        // Nothing needed for H.
      } else if (m == 1 && v.origin.r() == 0.0) {
        if (ma->Cmain[Hr])
          for (int z=0;z<v.nz();z++) {
            const double Czhr = ma->Cmain[Hr][z];
            const double ooop_Czhr = 1.0/(1.0+0.5*Czhr);
            const double dhrp = c*(-it(cmp,f[Ez],z+(v.nz()+1))/* /1.0 */);
            const double hrz = f[Hr][cmp][z] - f_pml[Hr][cmp][z];
            f_pml[Hr][cmp][z] += dhrp;
            f[Hr][cmp][z] += dhrp +
              ooop_Czhr*(c*(f[Ep][cmp][z+1]-f[Ep][cmp][z]) - Czhr*hrz);
          }
        else
          for (int z=0;z<v.nz();z++)
            f[Hr][cmp][z] += c*
              ((f[Ep][cmp][z+1]-f[Ep][cmp][z]) - it(cmp,f[Ez],z+(v.nz()+1))/* /1.0 */);
      } else {
        for (int r=0;r<=v.nr() && (int)(v.origin.r()*v.a+0.5) + r < m;r++) {
          const int ir = r*(v.nz()+1);
          for (int z=0;z<=v.nz();z++) f[Hr][cmp][z+ir] = 0;
          if (f_pml[Hr][cmp])
            for (int z=0;z<=v.nz();z++) f_pml[Hr][cmp][z+ir] = 0;
        }
      }
    }
  }
}

void fields::step_e() {
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      chunks[i]->step_e();
}

void fields_chunk::step_e() {
  const volume v = this->v;
  if (v.dim == d1) {
    DOCMP {
      if (ma->Cmain[Ex])
        for (int z=1;z<=v.nz();z++) {
          const double C = ma->Cmain[Ex][z];
          const double inveps_plus_C = ma->inveps[Ex][z]/(1.0+0.5*ma->inveps[Ex][z]*C);
          f[Ex][cmp][z] += inveps_plus_C*(-c*(f[Hy][cmp][z] - f[Hy][cmp][z-1])
                                          - C*f[Ex][cmp][z]);
        }
      else 
        for (int z=1;z<=v.nz();z++)
          f[Ex][cmp][z] += -c*ma->inveps[Ex][z]*(f[Hy][cmp][z] - f[Hy][cmp][z-1]);
    }
  } else if (v.dim == dcyl) {
    DOCMP {
      // Propogate Ep
      if (ma->Cmain[Ep] || ma->Cother[Ep])
        for (int r=rstart_1(v,m);r<=v.nr();r++) {
            const double oor = 1.0/((int)(v.origin.r()*v.a + 0.5) + r);
            const double mor = m*oor;
            const int ir = r*(v.nz()+1);
            const int irm1 = (r-1)*(v.nz()+1);
            for (int z=1;z<=v.nz();z++) {
              const double Czep = (ma->Cmain[Ep])?ma->Cmain[Ep][z+ir]:0;
              const double Crep = (ma->Cmain[Er])?ma->Cmain[Er][z+ir]:0;
              const double inveps_plus_Czep = ma->inveps[Ep][z+ir]/
                (1+.5*ma->inveps[Ep][z+ir]*Czep);
              const double inveps_plus_Crep = ma->inveps[Ep][z+ir]/
                (1+.5*ma->inveps[Ep][z+ir]*Crep);
              const double depz = inveps_plus_Czep*(c*(f[Hr][cmp][z+ir]-f[Hr][cmp][z+ir-1])
                                                    - Czep*f_pml[Ep][cmp][z+ir]);
              const double epr = f[Ep][cmp][z+ir] - f_pml[Ep][cmp][z+ir];
              f_pml[Ep][cmp][z+ir] += depz;
              f[Ep][cmp][z+ir] += depz +
                inveps_plus_Crep*(c*(-(f[Hz][cmp][z+ir]-f[Hz][cmp][z+irm1])) - Crep*epr);
            }
          }
      else
        for (int r=rstart_1(v,m);r<=v.nr();r++) {
            const int ir = r*(v.nz()+1);
            const int irm1 = (r-1)*(v.nz()+1);
            for (int z=1;z<=v.nz();z++)
              f[Ep][cmp][z+ir] += c*ma->inveps[Ep][z+ir]*
                ((f[Hr][cmp][z+ir]-f[Hr][cmp][z+ir-1])
                 - (f[Hz][cmp][z+ir]-f[Hz][cmp][z+irm1]));
          }
      // Propogate Ez
      if (ma->Cother[Ez])
        for (int r=rstart_1(v,m);r<=v.nr();r++) {
          double oor = 1.0/((int)(v.origin.r()*v.a + 0.5) + r);
          double mor = m*oor;
          const int ir = r*(v.nz()+1);
          const int irm1 = (r-1)*(v.nz()+1);
          for (int z=0;z<v.nz();z++) {
            const double Crez = ma->Cother[Ez][z+ir];
            const double inveps_plus_Crez = ma->inveps[Ez][z+ir]/
              (1+.5*ma->inveps[Ez][z+ir]*Crez);

            const double dezr = inveps_plus_Crez*
              (c*(f[Hp][cmp][z+ir]*((int)(v.origin.r()*v.a+0.5) + r+0.5)-
                  f[Hp][cmp][z+irm1]*((int)(v.origin.r()*v.a+0.5) + r-0.5))*oor
               - Crez*f_pml[Ez][cmp][z+ir]);
            const double ezp = f[Ez][cmp][z+ir]-f_pml[Ez][cmp][z+ir];
            f_pml[Ez][cmp][z+ir] += dezr;
            f[Ez][cmp][z+ir] += dezr +
              c*ma->inveps[Ez][z+ir]*(-it(cmp,f[Hr],z+ir)*mor);
          }
        }
      else
        for (int r=rstart_1(v,m);r<=v.nr();r++) {
          double oor = 1.0/((int)(v.origin.r()*v.a + 0.5) + r);
          double mor = m*oor;
          const int ir = r*(v.nz()+1);
          const int irm1 = (r-1)*(v.nz()+1);
          for (int z=0;z<v.nz();z++)
            f[Ez][cmp][z+ir] += c*ma->inveps[Ez][z+ir]*
              ((f[Hp][cmp][z+ir]*((int)(v.origin.r()*v.a+0.5) + r+0.5)-
                f[Hp][cmp][z+irm1]*((int)(v.origin.r()*v.a+0.5) + r-0.5))*oor
               - it(cmp,f[Hr],z+ir)*mor);
        }
      // Propogate Er
      if (ma->Cother[Er])
        for (int r=rstart_0(v,m);r<v.nr();r++) {
            double oorph = 1.0/((int)(v.origin.r()*v.a+0.5) + r+0.5);
            double morph = m*oorph;
            const int ir = r*(v.nz()+1);
            const int irp1 = (r+1)*(v.nz()+1);
            for (int z=1;z<=v.nz();z++) {
              const double Czer = ma->Cother[Er][z+ir];
              const double inveps_plus_Czer = ma->inveps[Er][z+ir]/
                (1+.5*ma->inveps[Er][z+ir]*Czer);
              double derp = c*ma->inveps[Er][z+ir]*(it(cmp,f[Hz],z+ir)*morph);
              double erz = f[Er][cmp][z+ir] - f_pml[Er][cmp][z+ir];
              f_pml[Er][cmp][z+ir] += derp;
              f[Er][cmp][z+ir] += derp + inveps_plus_Czer*
                (-c*(f[Hp][cmp][z+ir]-f[Hp][cmp][z+ir-1]) - Czer*erz);
            }
          }
      else
        for (int r=rstart_0(v,m);r<v.nr();r++) {
            double oorph = 1.0/((int)(v.origin.r()*v.a+0.5) + r+0.5);
            double morph = m*oorph;
            const int ir = r*(v.nz()+1);
            const int irp1 = (r+1)*(v.nz()+1);
            for (int z=1;z<=v.nz();z++)
              f[Er][cmp][z+ir] += c*ma->inveps[Er][z+ir]*
                (it(cmp,f[Hz],z+ir)*morph - (f[Hp][cmp][z+ir]-f[Hp][cmp][z+ir-1]));
          }
      // Deal with annoying r==0 boundary conditions...
      if (m == 0 && v.origin.r() == 0.0) {
        for (int z=0;z<=v.nz();z++)
          f[Ez][cmp][z] += c*ma->inveps[Ez][z]*(f[Hp][cmp][z] + it(cmp,f[Hr],z)*m);
      } else if (m == 1 && v.origin.r() == 0.0) {
        if (ma->Cmain[Ep])
          for (int z=1;z<=v.nz();z++) {
            const double Czep = ma->Cmain[Ep][z];
            const double inveps_plus_Czep = ma->inveps[Ep][z]/
              (1+.5*ma->inveps[Ep][z]*Czep);
            const double depz = inveps_plus_Czep*(c*(f[Hr][cmp][z]-f[Hr][cmp][z-1])
                                                  - Czep*f_pml[Ep][cmp][z]);
            const double epr = f[Ep][cmp][z] - f_pml[Ep][cmp][z];
            f_pml[Ep][cmp][z] += depz;
            f[Ep][cmp][z] += depz +
              c*ma->inveps[Ep][z]*(-f[Hz][cmp][z]*2.0);
          }
        else
          for (int z=1;z<=v.nz();z++)
            f[Ep][cmp][z] += c*ma->inveps[Ep][z]*
              ((f[Hr][cmp][z]-f[Hr][cmp][z-1]) - f[Hz][cmp][z]*2.0);
      } else {
        for (int r=0;r<=v.nr() && (int)(v.origin.r()*v.a+0.5) + r < m;r++) {
          const int ir = r*(v.nz()+1);
          for (int z=0;z<=v.nz();z++) f[Ep][cmp][z+ir] = 0;
          if (f_pml[Ep][cmp])
            for (int z=0;z<=v.nz();z++) f_pml[Ep][cmp][z+ir] = 0;
          for (int z=0;z<=v.nz();z++) f[Ez][cmp][z+ir] = 0;
          if (f_pml[Ez][cmp])
            for (int z=0;z<=v.nz();z++) f_pml[Ez][cmp][z+ir] = 0;
        }
      }
    }
  } else {
    abort("Unsupported dimension.\n");
  }
}

#include "config.h"
#ifdef HAVE_MPI
#include <mpi.h>
#endif

void fields::step_boundaries(field_type ft) {
  // First copy outgoing data to buffers...
  int *wh = new int[num_chunks];

  for (int i=0;i<num_chunks;i++) wh[i] = 0;
  for (int i=0;i<num_chunks;i++)
    for (int j=0;j<num_chunks;j++)
      if (chunks[j]->is_mine()) {
        const int pair = j+i*num_chunks;
        for (int n=0;n<comm_sizes[ft][pair];n++) {
          DOCMP {
            double stupid = *(chunks[j]->connections[ft][Outgoing][cmp][wh[j]]);
            comm_blocks[ft][pair][n*2+cmp] = stupid;
          }
          wh[j]++;
        }
      }
  // Communicate the data around!
#if 0 // This is the blocking version, which should always be safe!
  for (int noti=0;noti<num_chunks;noti++)
    for (int j=0;j<num_chunks;j++) {
      const int i = (noti+j)%num_chunks;
      const int pair = j+i*num_chunks;
      DOCMP {
        send(chunks[j]->n_proc(), chunks[i]->n_proc(),
             comm_blocks[ft][pair], comm_sizes[ft][pair]*2);
      }
    }
#endif
#ifdef HAVE_MPI
  MPI_Request *reqs = new MPI_Request[num_chunks*4];
  MPI_Status *stats = new MPI_Status[num_chunks*4];
  int reqnum = 0;
  for (int noti=0;noti<num_chunks;noti++)
    for (int j=0;j<num_chunks;j++) {
      const int i = (noti+j)%num_chunks;
      if (chunks[j]->n_proc() != chunks[i]->n_proc()) {
        const int pair = j+i*num_chunks;
        if (comm_sizes[ft][pair] > 0) {
          if (chunks[j]->is_mine())
            MPI_Isend(comm_blocks[ft][pair], comm_sizes[ft][pair]*2,
                      MPI_DOUBLE, chunks[i]->n_proc(), pair,
                      MPI_COMM_WORLD, &reqs[reqnum++]);
          if (chunks[i]->is_mine())
            MPI_Irecv(comm_blocks[ft][pair], comm_sizes[ft][pair]*2,
                      MPI_DOUBLE, chunks[j]->n_proc(), pair,
                      MPI_COMM_WORLD, &reqs[reqnum++]);
        }
      }
    }
  if (reqnum > 0) MPI_Waitall(reqnum, reqs, stats);
  delete[] reqs;
  delete[] stats;
#endif
  
  // Finally, copy incoming data to the fields themselves!
  for (int i=0;i<num_chunks;i++) wh[i] = 0;
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      for (int j=0;j<num_chunks;j++) {
        const int pair = j+i*num_chunks;
        for (int n=0;n<comm_sizes[ft][pair];n++) {
          complex<double> val = chunks[i]->connection_phases[ft][wh[i]]*
            complex<double>(comm_blocks[ft][pair][n*2],
                            comm_blocks[ft][pair][n*2+1]);
          *(chunks[i]->connections[ft][Incoming][0][wh[i]]) = real(val);
          *(chunks[i]->connections[ft][Incoming][1][wh[i]]) = imag(val);
          wh[i]++;
        }
      }
  delete[] wh;
}

void fields::step_h_source() {
  const double tim = time();
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      chunks[i]->step_h_source(chunks[i]->h_sources, tim);
}

void fields_chunk::step_h_source(const src *s, double time) {
  if (s == NULL) return;
  complex<double> A = s->get_amplitude_at_time(time);
  if (A == 0.0) {
    step_h_source(s->next, time);
    return;
  }
  for (int c=0;c<10;c++)
    if (v.has_field((component)c) && is_magnetic((component)c)) {
      f[c][0][s->i] += real(A*s->A[c]);
      if (!is_real) f[c][1][s->i] += imag(A*s->A[c]);
    }
  step_h_source(s->next, time);
}

void fields::step_e_source() {
  const double tim = time()+0.5*inva*c;
  for (int i=0;i<num_chunks;i++)
    if (chunks[i]->is_mine())
      chunks[i]->step_e_source(chunks[i]->e_sources, tim);
}

void fields_chunk::step_e_source(const src *s, double time) {
  if (s == NULL) return;
  complex<double> A = s->get_amplitude_at_time(time);
  if (A == 0.0) {
    step_e_source(s->next, time);
    return;
  }
  for (int c=0;c<10;c++)
    if (v.has_field((component)c) && is_electric((component)c)) {
      f[c][0][s->i] += real(A*s->A[c]);
      if (!is_real) f[c][1][s->i] += imag(A*s->A[c]);
    }
  step_e_source(s->next, time);
}
