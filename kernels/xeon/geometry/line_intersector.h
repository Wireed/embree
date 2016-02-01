// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "../../common/ray.h"
#include "filter.h"

namespace embree
{
  namespace isa
  {
    static __forceinline std::pair<float,float> intersect_half_plane(const Vec3fa& ray_org, const Vec3fa& ray_dir, const Vec3fa& N, const Vec3fa& P)
    {
      Vec3fa O = Vec3fa(ray_org) - P;
      Vec3fa D = Vec3fa(ray_dir);
      float ON = dot(O,N);
      float DN = dot(D,N);
      float t = -ON*rcp(abs(DN) < min_rcp_input ? min_rcp_input : DN );
      float lower = select(DN < 0.0f, float(neg_inf), t);
      float upper = select(DN < 0.0f, t, float(pos_inf));
      return std::make_pair(lower,upper);
    }

    static __forceinline bool intersect_cone(const Vec3fa& org_i, const Vec3fa& dir, 
                                             const Vec3fa& v0_i, const float r0, 
                                             const Vec3fa& v1_i, const float r1,
                                             float& t0_o, //float& u0_o, Vec3fa& Ng0_o,
                                             float& t1_o)
      
    {
      const float tb = dot(0.5f*(v0_i+v1_i)-org_i,normalize(dir));
      const Vec3fa org = org_i+tb*dir;
      const Vec3fa v0 = v0_i-org;
      const Vec3fa v1 = v1_i-org;
      
      const float rl = rcp_length(v1-v0);
      const Vec3fa P0 = v0, dP = (v1-v0)*rl;
      const float dr = (r1-r0)*rl;
      const Vec3fa O = -P0, dO = dir;
      
      const float dOdO = dot(dO,dO);
      const float OdO = dot(dO,O);
      const float OO = dot(O,O);
      const float dOz = dot(dP,dO);
      const float Oz = dot(dP,O);
      
      const float R = r0 + Oz*dr;          
      const float A = dOdO - sqr(dOz) * (1.0f+sqr(dr));
      const float B = 2.0f * (OdO - dOz*(Oz + R*dr));
      const float C = OO - (sqr(Oz) + sqr(R));
      
      const float D = B*B - 4.0f*A*C;
      if (D < 0.0f) return false;
      
      const float Q = sqrt(D);
      const float rcp_2A = rcp(2.0f*A);
      t0_o = (-B-Q)*rcp_2A;
      t1_o = (-B+Q)*rcp_2A;
      
      //u0_o = (Oz+t0_o*dOz)*rl;
      //const Vec3fa Pr = t0_o*dir;
      //const Vec3fa Pl = v0 + u0_o*(v1-v0);
      //Ng0_o = Pr-Pl;
      t0_o += tb;
      t1_o += tb;
      return true;
    }

    static __forceinline bool intersect_cone(const Vec3fa& dir, 
                                             const Vec3fa& v0, const float r0, 
                                             const Vec3fa& v1, const float r1,
                                             float& t0_o,
                                             float& t1_o,
                                             Vec3fa& Ng)
      
    {
      const float rl = rcp_length(v1-v0);
      const Vec3fa P0 = v0, dP = (v1-v0)*rl;
      const float dr = (r1-r0)*rl;
      const Vec3fa O = -P0, dO = dir;
      
      const float dOdO = dot(dO,dO);
      const float OdO = dot(dO,O);
      const float OO = dot(O,O);
      const float dOz = dot(dP,dO);
      const float Oz = dot(dP,O);
      
      const float R = r0 + Oz*dr;          
      const float A = dOdO - sqr(dOz) * (1.0f+sqr(dr));
      const float B = 2.0f * (OdO - dOz*(Oz + R*dr));
      const float C = OO - (sqr(Oz) + sqr(R));
      
      const float D = B*B - 4.0f*A*C;
      if (D < 0.0f) return false;
      
      const float Q = sqrt(D);
      if (unlikely(A < min_rcp_input)) {
        t0_o = float(neg_inf);
        t1_o = float(pos_inf);
        return true;
      }

      const float rcp_2A = 0.5f*rcp(A);
      t0_o = (-B-Q)*rcp_2A;
      t1_o = (-B+Q)*rcp_2A;
      
      float u0_o = (Oz+t0_o*dOz)*rl;
      const Vec3fa Pr = t0_o*dir;
      const Vec3fa Pl = v0 + u0_o*(v1-v0);
      Ng = Pr-Pl;
      //t0_o += tb;
      //t1_o += tb;
      return true;
    }

    __forceinline float distance_f(const Vec3fa& p,Vec3fa& q0, Vec3fa& q1, Vec3fa& Ng,
                                   const Vec3fa& p0, const Vec3fa& n0, const float r0,
                                   const Vec3fa& p1, const Vec3fa& n1, const float r1)
    {
      const Vec3fa N = cross(p-p0,p1-p0);
#if defined (__AVX__)
      const vfloat<8> p0p1 = vfloat<8>(vfloat<4>(p0),vfloat<4>(p1));
      const vfloat<8> n0n1 = vfloat<8>(vfloat<4>(n0),vfloat<4>(n1));
      const vfloat<8> r0r1 = vfloat<8>(vfloat<4>(r0),vfloat<4>(r1));
      const vfloat<8> NN   = vfloat<8>(vfloat<4>(N));
      const vfloat<8> q0q1 = p0p1 + r0r1*normalize(cross(n0n1,NN));
      q0 = (Vec3fa)extract4<0>(q0q1);
      q1 = (Vec3fa)extract4<1>(q0q1);
#else
      q0 = p0+r0*normalize(cross(n0,N));
      q1 = p1+r1*normalize(cross(n1,N));
#endif
      Ng = normalize(cross(q1-q0,N));
      return dot(p-q0,Ng);
    }

     __forceinline float distance_f(const Vec3fa& p,
                                   const Vec3fa& p0, const Vec3fa& n0, const float r0,
                                   const Vec3fa& p1, const Vec3fa& n1, const float r1)
    {
      Vec3fa q0; Vec3fa q1; Vec3fa Ng;
      return distance_f(p,q0,q1,Ng,p0,n0,r0,p1,n1,r1);
    }

    __forceinline float pow_3_2(float x) {
      return x*sqrt(x);
    }
    
    __forceinline Vec3fa normalize_dx(const Vec3fa& p, const Vec3fa& dpdx)
    {
      float pp = dot(p,p);
      float x = pp*dpdx.x - p.x*(p.x*dpdx.x + p.y*dpdx.y + p.z*dpdx.z);
      float y = pp*dpdx.y - p.y*(p.x*dpdx.x + p.y*dpdx.y + p.z*dpdx.z);
      float z = pp*dpdx.z - p.z*(p.x*dpdx.x + p.y*dpdx.y + p.z*dpdx.z);
      return Vec3fa(x,y,z)/pow_3_2(pp);
    }

    __forceinline Vec3fa grad_distance_f(const Vec3fa& p, 
                                         const Vec3fa& p0, const Vec3fa& n0, const float r0,
                                         const Vec3fa& p1, const Vec3fa& n1, const float r1)
    {
#if 0
      const float e = 0.001f;
      const float v = distance_f(p,p0,n0,r0,p1,n1,r1);
      const float x = distance_f(p+Vec3fa(e,0,0),p0,n0,r0,p1,n1,r1);
      const float y = distance_f(p+Vec3fa(0,e,0),p0,n0,r0,p1,n1,r1);
      const float z = distance_f(p+Vec3fa(0,0,e),p0,n0,r0,p1,n1,r1);
      return Vec3fa(x-v,y-v,z-v)/e;
#else
      const Vec3fa N    = cross(p-p0,p1-p0);
      const Vec3fa dNdx = cross(Vec3fa(1,0,0),p1-p0);
      const Vec3fa dNdy = cross(Vec3fa(0,1,0),p1-p0);
      const Vec3fa dNdz = cross(Vec3fa(0,0,1),p1-p0);

      const Vec3fa N0    = cross(n0,N);
      const Vec3fa dN0dx = cross(n0,dNdx); 
      const Vec3fa dN0dy = cross(n0,dNdy); 
      const Vec3fa dN0dz = cross(n0,dNdz); 

      const Vec3fa N1    = cross(n1,N);
      const Vec3fa dN1dx = cross(n1,dNdx); 
      const Vec3fa dN1dy = cross(n1,dNdy); 
      const Vec3fa dN1dz = cross(n1,dNdz); 

      const Vec3fa q0    = p0+r0*normalize(N0);
      const Vec3fa dq0dx = r0*normalize_dx(N0,dN0dx);
      const Vec3fa dq0dy = r0*normalize_dx(N0,dN0dy);
      const Vec3fa dq0dz = r0*normalize_dx(N0,dN0dz);

      const Vec3fa q1    = p1+r1*normalize(N1);
      const Vec3fa dq1dx = r1*normalize_dx(N1,dN1dx);
      const Vec3fa dq1dy = r1*normalize_dx(N1,dN1dy);
      const Vec3fa dq1dz = r1*normalize_dx(N1,dN1dz);

      const Vec3fa K    = cross(q1-q0,N);
      const Vec3fa dKdx = cross(dq1dx-dq0dx,N) + cross(q1-q0,dNdx);
      const Vec3fa dKdy = cross(dq1dy-dq0dy,N) + cross(q1-q0,dNdy);
      const Vec3fa dKdz = cross(dq1dz-dq0dz,N) + cross(q1-q0,dNdz);

      const Vec3fa Ng    = normalize(K);
      const Vec3fa dNgdx = normalize_dx(K,dKdx); 
      const Vec3fa dNgdy = normalize_dx(K,dKdy); 
      const Vec3fa dNgdz = normalize_dx(K,dKdz); 

      const float f    = dot(p-q0,Ng);
      const float dfdx = dot(Vec3fa(1,0,0)-dq0dx,Ng) + dot(p-q0,dNgdx); 
      const float dfdy = dot(Vec3fa(0,1,0)-dq0dy,Ng) + dot(p-q0,dNgdy); 
      const float dfdz = dot(Vec3fa(0,0,1)-dq0dz,Ng) + dot(p-q0,dNgdz); 

      return Vec3fa(dfdx,dfdy,dfdz);
#endif
    }

    static __noinline bool intersect_fill_cone(const Ray& ray,
                                               const Vec3fa& p0_i, const Vec3fa& n0, const float r0,
                                               const Vec3fa& p1_i, const Vec3fa& n1, const float r1,
                                               float& u, float& t, Vec3fa& Ng)
    {
      //PING;
      STAT(Stat::get().user[0]++); 

      /* move closer to geometry to make intersection stable */
      const Vec3fa C = 0.5f*(p0_i+p1_i);
      const float tb = dot(C-ray.org,normalize(ray.dir));
      const Vec3fa org = ray.org+tb*normalize(ray.dir);
      const float dirlen = length(ray.dir);
      const Vec3fa dir = ray.dir;
      const Vec3fa p0 = p0_i-org;
      const Vec3fa p1 = p1_i-org;
      const float ray_tnear = ray.tnear-tb;
      const float ray_tfar = ray.tfar-tb;

      //PRINT(ray_tnear);
      //PRINT(ray_tfar);
      //PRINT(r0);
      //PRINT(r1);
      float maxR = max(r0,r1);
      ////PRINT(maxR);
      float t_term = 0.00001f*maxR;
      float t_err = 0.0001f*maxR;
      const float r01 = maxR; //+t_err;
      float tc_lower,tc_upper;
      Vec3fa NgA;
      if (!intersect_cone(dir,p0,r01,p1,r01,tc_lower,tc_upper,NgA)) {
        STAT(Stat::get().user[1]++); 
        return false;
      }
      //PRINT(tc_lower);
      //PRINT(tc_upper);

      auto tp0 = intersect_half_plane(zero,dir,+n0,p0);
      auto tp1 = intersect_half_plane(zero,dir,-n1,p1);
      //PRINT(tp0.first);
      //PRINT(tp0.second);
      //PRINT(tp1.first);
      //PRINT(tp1.second);
      
      float td_lower = max(ray_tnear,tc_lower,tp0.first ,tp1.first );
      float td_upper = min(ray_tfar,tc_upper,tp0.second,tp1.second);
      //PRINT(td_lower);
      //PRINT(td_upper);
      if (td_lower > td_upper) {
        STAT(Stat::get().user[2]++); 
        return false;
      }
      
      tc_lower = max(ray_tnear,tp0.first ,tp1.first );
      tc_upper = min(ray_tfar,tc_upper+0.1f*maxR,tp0.second,tp1.second);

#if 0
      if (tc_lower > ray_tnear-tb && tc_lower < ray_tfar-tb) 
      {
        u = 0.0f;
        t = tb+tc_lower;
        Ng = NgA;
        return true;
      }
#endif

      const Vec3fa p1p0 = p1-p0;
      const Vec3fa norm_p1p0 = normalize(p1p0);
      
      float A0 = abs(dot(norm_p1p0,normalize(n0)));
      float A1 = abs(dot(norm_p1p0,normalize(n1)));
      float rcpMaxDerivative = max(0.01f,min(A0,A1))/dirlen;

      ////PRINT(tc_lower);
      ////PRINT(tc_upper);
      
      STAT(Stat::get().user[3]++);
      t = td_lower; float dt = inf;
      Vec3fa p = t*dir;
      float inout = 1.0f;
      
      for (size_t i=0;; i++) 
      {
        //PRINT(i);
        STAT(Stat::get().user[4]++); 
        if (unlikely(i == 2000)) {
          STAT(Stat::get().user[5]++); 
          //PRINT("miss1");
          return false;
        }
        if (unlikely(t > tc_upper)) {
          STAT(Stat::get().user[6]++); 
          //PRINT("break1");
          break;
        }
        Vec3fa q0,q1,Ng;
        dt = distance_f(p,q0,q1,Ng,p0,n0,r0,p1,n1,r1);
        if (i == 0 && dt < 0.0f) inout = -1.0f;
        dt *= inout;
        //PRINT(dt);
        if (unlikely(std::isnan(dt))) {
          //PRINT("miss2");
          dt = 1.0f;
          //return false;
        }
        t += rcpMaxDerivative*dt;
        //if (p == t*d) break;
        p = t*dir;
        //PRINT(t);
        //PRINT(p);
        if (unlikely(abs(dt) < t_term)) {
          //PRINT("break2");
          STAT(Stat::get().user[7]++); 
          u = dot(p-q0,q1-q0)*rcp_length2(q1-q0);
          break;
        }
      }
      if (t < tc_lower || t > tc_upper) {
        //PRINT("miss3");
        return false;
      }
      
      const Vec3fa N = cross(p-p0,p1p0);
      const Vec3fa Ng0 = normalize(cross(n0,N));
      const Vec3fa Ng1 = normalize(cross(n1,N));
      const Vec3fa q0 = p0+r0*Ng0;
      const Vec3fa q1 = p1+r1*Ng1;
      Ng = normalize(cross(q1-q0,N));
      const Vec3fa P = (1.0f-u)*q0 + u*q1;
      t = tb+dot(q0,Ng)/dot(dir,Ng);

      Ng = grad_distance_f(p,p0,n0,r0,p1,n1,r1);
      //PRINT("hit");
      //PRINT(t);
      //PRINT(Ng);
      //PRINT(P);
      return true;
    }
        
    template<int M>
      struct LineIntersectorHitM
      {
        __forceinline LineIntersectorHitM() {}
        
        __forceinline LineIntersectorHitM(const vfloat<M>& u, const vfloat<M>& v, const vfloat<M>& t, const Vec3<vfloat<M>>& Ng)
          : vu(u), vv(v), vt(t), vNg(Ng) {}
        
        __forceinline void finalize() {}
        
        __forceinline Vec2f uv (const size_t i) const { return Vec2f(vu[i],vv[i]); }
        __forceinline float t  (const size_t i) const { return vt[i]; }
        __forceinline Vec3fa Ng(const size_t i) const { return Vec3fa(vNg.x[i],vNg.y[i],vNg.z[i]); }
        
      public:
        vfloat<M> vu;
        vfloat<M> vv;
        vfloat<M> vt;
        Vec3<vfloat<M>> vNg;
      };
    
    template<int M>
      struct LineIntersector1
      {
        typedef Vec3<vfloat<M>> Vec3vfM;
        typedef Vec4<vfloat<M>> Vec4vfM;
        
        struct Precalculations
        {
          __forceinline Precalculations (const Ray& ray, const void* ptr)
          {
            const float s = rsqrt(dot(ray.dir,ray.dir));
            depth_scale = s;
            ray_space = frame(s*ray.dir).transposed();
          }
          
          vfloat<M> depth_scale;
          LinearSpace3<Vec3vfM> ray_space;
        };

        static __forceinline vbool<M> intersect_sphere(vbool<M> valid, 
                                                       Ray& ray, const Vec4vfM& v0,
                                                       vfloat<M>& t_o,
                                                       Vec3<vfloat<M>>& Ng_o)
        {
          Vec3<vfloat<M>> P0 = Vec3<vfloat<M>>(v0.x,v0.y,v0.z);
          vfloat<M> r0 = v0.w;

          const Vec3<vfloat<M>> O = Vec3<vfloat<M>>(ray.org)-P0, dO = ray.dir;
          
          const vfloat<M> A = dot(dO,dO);
          const vfloat<M> B = 2.0f * dot(dO,O);
          const vfloat<M> C = dot(O,O) - sqr(r0);
          
          const vfloat<M> D = B*B - 4.0f*A*C;
          valid &= D >= 0.0f;
          if (none(valid)) return false;
          
          const vfloat<M> Q = sqrt(D);
          //const vfloat<M> t0 = (-B-Q)*rcp2A;
          //const vfloat<M> t1 = (-B+Q)*rcp2A;
          const vfloat<M> t0 = (-B-Q)/(2.0f*A);
          const vfloat<M> t = t0;
          valid &= (ray.tnear < t) & (t < ray.tfar);
          if (unlikely(none(valid))) return false;
          
          /* update hit information */
          const Vec3<vfloat<M>> Pr = Vec3<vfloat<M>>(ray.org) + t*Vec3<vfloat<M>>(ray.dir);
          t_o = t;
          Ng_o = Pr-P0;
          return valid;
        }

        static __forceinline vbool<M> intersect_cone(vbool<M> valid, 
                                                     Ray& ray, const Vec4vfM& v0, const Vec4vfM& v1,
                                                     vfloat<M>& t_o,
                                                     vfloat<M>& u_o,
                                                     Vec3<vfloat<M>>& Ng_o)
        {
          Vec4<vfloat<M>> a = v0;
          Vec4<vfloat<M>> b = v1;
          Vec3<vfloat<M>> a3(v0.x,v0.y,v0.z);
          Vec3<vfloat<M>> b3(v1.x,v1.y,v1.z);

          //valid &= length(b3-a3) > 1E-5f;

          const vfloat<M> rl = rcp_length(b3-a3);
          const Vec3<vfloat<M>> P0 = a3, dP = (b3-a3)*rl;
          const vfloat<M> r0 = a.w, dr = (b.w-a.w)*rl;
          const Vec3<vfloat<M>> O = Vec3<vfloat<M>>(ray.org)-P0, dO = ray.dir;
          
          const vfloat<M> dOdO = dot(dO,dO);
          const vfloat<M> OdO = dot(dO,O);
          const vfloat<M> OO = dot(O,O);
          const vfloat<M> dOz = dot(dP,dO);
          const vfloat<M> Oz = dot(dP,O);

          const vfloat<M> R = r0 + Oz*dr;          
          const vfloat<M> A = dOdO - sqr(dOz) * (1.0f+sqr(dr));
          const vfloat<M> B = 2.0f * (OdO - dOz*(Oz + R*dr));
          const vfloat<M> C = OO - (sqr(Oz) + sqr(R));
          
          const vfloat<M> D = B*B - 4.0f*A*C;
          valid &= D >= 0.0f;
          if (none(valid)) return false;
          
          const vfloat<M> Q = sqrt(D);
          //const vfloat<M> t0 = (-B-Q)*rcp2A;
          //const vfloat<M> t1 = (-B+Q)*rcp2A;
          const vfloat<M> t0 = (-B-Q)/(2.0f*A);
          const vfloat<M> u0 = Oz+t0*dOz;
          const vfloat<M> t = t0;
          const vfloat<M> u = u0*rl;
          //valid &= (ray.tnear < t) & (t < ray.tfar);// & (0.0f <= u) & (u <= 1.0f); 
          //if (unlikely(none(valid))) return false;
          
          /* update hit information */
          const Vec3<vfloat<M>> Pr = Vec3<vfloat<M>>(ray.org) + t*Vec3<vfloat<M>>(ray.dir);
          const Vec3<vfloat<M>> Pl = a3 + u*(b3-a3);
          t_o = t;
          u_o = u;
          Ng_o = Pr-Pl;
          return valid;
        }

         static __forceinline vbool<M> intersect_cylinder(vbool<M> valid, 
                                                          Ray& ray, const Vec3vfM& v0, const Vec3vfM& v1, const vfloat<M>& R,
                                                          vfloat<M>& t_o,
                                                          vfloat<M>& u_o,
                                                          Vec3<vfloat<M>>& Ng_o)
        {
          const vfloat<M> rl = rcp_length(v1-v0);
          const Vec3<vfloat<M>> dP = (v1-v0)*rl;
          const Vec3<vfloat<M>> O = Vec3<vfloat<M>>(ray.org)-v0, dO = ray.dir;
          
          const vfloat<M> dOdO = dot(dO,dO);
          const vfloat<M> OdO = dot(dO,O);
          const vfloat<M> OO = dot(O,O);
          const vfloat<M> dOz = dot(dP,dO);
          const vfloat<M> Oz = dot(dP,O);

          const vfloat<M> A = dOdO - sqr(dOz);
          const vfloat<M> B = 2.0f * (OdO - dOz*Oz);
          const vfloat<M> C = OO - (sqr(Oz) + sqr(R));
          
          const vfloat<M> D = B*B - 4.0f*A*C;
          valid &= D >= 0.0f;
          if (none(valid)) return false;
          
          const vfloat<M> Q = sqrt(D);
          //const vfloat<M> t0 = (-B-Q)*rcp2A;
          //const vfloat<M> t1 = (-B+Q)*rcp2A;
          const vfloat<M> t0 = (-B-Q)/(2.0f*A);
          const vfloat<M> u0 = Oz+t0*dOz;
          const vfloat<M> t = t0;
          const vfloat<M> u = u0*rl;
          //valid &= (ray.tnear < t) & (t < ray.tfar);// & (0.0f <= u) & (u <= 1.0f); 
          //if (unlikely(none(valid))) return false;
          
          /* update hit information */
          const Vec3<vfloat<M>> Pr = Vec3<vfloat<M>>(ray.org) + t*Vec3<vfloat<M>>(ray.dir);
          const Vec3<vfloat<M>> Pl = v0 + u*(v1-v0);
          t_o = t;
          u_o = u;
          Ng_o = Pr-Pl;
          return valid;
        }

        static __forceinline bool intersect_cone(const Vec3fa& org_i, const Vec3fa& dir, 
                                                 const Vec3fa& v0_i, const float r0, 
                                                 const Vec3fa& v1_i, const float r1,
                                                 float& t0_o, float& u0_o, Vec3fa& Ng0_o,
                                                 float& t1_o)

        {
          const float tb = dot(0.5f*(v0_i+v1_i)-org_i,normalize(dir));
          const Vec3fa org = org_i+tb*dir;
          const Vec3fa v0 = v0_i-org;
          const Vec3fa v1 = v1_i-org;

          const float rl = rcp_length(v1-v0);
          const Vec3fa P0 = v0, dP = (v1-v0)*rl;
          const float dr = (r1-r0)*rl;
          const Vec3fa O = -P0, dO = dir;
          
          const float dOdO = dot(dO,dO);
          const float OdO = dot(dO,O);
          const float OO = dot(O,O);
          const float dOz = dot(dP,dO);
          const float Oz = dot(dP,O);

          const float R = r0 + Oz*dr;          
          const float A = dOdO - sqr(dOz) * (1.0f+sqr(dr));
          const float B = 2.0f * (OdO - dOz*(Oz + R*dr));
          const float C = OO - (sqr(Oz) + sqr(R));
          
          const float D = B*B - 4.0f*A*C;
          if (D < 0.0f) return false;
          
          const float Q = sqrt(D);
          const float rcp_2A = rcp(2.0f*A);
          t0_o = (-B-Q)*rcp_2A;
          t1_o = (-B+Q)*rcp_2A;

          u0_o = (Oz+t0_o*dOz)*rl;
          const Vec3fa Pr = t0_o*dir;
          const Vec3fa Pl = v0 + u0_o*(v1-v0);
          Ng0_o = Pr-Pl;
          t0_o += tb;
          t1_o += tb;
          return true;
        }

        static __forceinline std::pair<vfloat<M>,vfloat<M>> intersect_half_plane(Ray& ray, const Vec3<vfloat<M>>& N, const Vec3<vfloat<M>>& P)
        {
          Vec3<vfloat<M>> O = Vec3<vfloat<M>>(ray.org) - P;
          Vec3<vfloat<M>> D = Vec3<vfloat<M>>(ray.dir);
          vfloat<M> ON = dot(O,N);
          vfloat<M> DN = dot(D,N);
          vfloat<M> t = -ON*rcp(DN);
          vfloat<M> lower = select(DN < 0.0f, float(neg_inf), t);
          vfloat<M> upper = select(DN < 0.0f, t, float(pos_inf));
          return std::make_pair(lower,upper);
        }

        static __forceinline std::pair<float,float> intersect_half_plane(const Vec3fa& ray_org, const Vec3fa& ray_dir, const Vec3fa& N, const Vec3fa& P)
        {
          Vec3fa O = Vec3fa(ray_org) - P;
          Vec3fa D = Vec3fa(ray_dir);
          float ON = dot(O,N);
          float DN = dot(D,N);
          float t = -ON*rcp(DN);
          float lower = select(DN < 0.0f, float(neg_inf), t);
          float upper = select(DN < 0.0f, t, float(pos_inf));
          return std::make_pair(lower,upper);
        }

        static __forceinline Vec3fa distributator(const Vec3fa& A, const Vec3fa& B, const Vec3fa& C) {
          return dot(A,B)*C - A*dot(B,C);
        }

        static __forceinline float f(const float u, 
                                     const Vec3fa& d,
                                     const Vec3fa& p0, const Vec3fa& n0, const float r0,
                                     const Vec3fa& p1, const Vec3fa& n1, const float r1)
        {
          const Vec3fa ps = (1.0f-u)*p0 + u*p1;
          const Vec3fa ns = (1.0f-u)*n0 + u*n1;
          const float  rs = (1.0f-u)*r0 + u*r1;
          const Vec3fa A = distributator(ps,ns,d);
          const float  B = rs*dot(ns,d);
          return dot(A,A) - sqr(B);
        }

        static __forceinline float dfds(const float u,
                                        const Vec3fa& d,
                                        const Vec3fa& p0, const Vec3fa& n0, const float r0,
                                        const Vec3fa& p1, const Vec3fa& n1, const float r1)
        {
          const Vec3fa ps = (1.0f-u)*p0 + u*p1, dps = p1-p0;
          const Vec3fa ns = (1.0f-u)*n0 + u*n1, dns = n1-n0;
          const float  rs = (1.0f-u)*r0 + u*r1, drs = r1-r0;
          const Vec3fa A  = distributator(ps,ns,d);
          const Vec3fa dA = distributator(dps,ns,d)+distributator(ps,dns,d);
          const float  B  = rs*dot(ns,d);
          const float  dB = drs*dot(ns,d) + rs*dot(dns,d);
          return 2.0f*dot(dA,A) - 2.0f*dB*B;
        }

        static __forceinline bool intersect_iterative1(const Vec3fa& d,
                                                      const Vec3fa& p0, const Vec3fa& n0, const float r0,
                                                      const Vec3fa& p1, const Vec3fa& n1, const float r1,
                                                      float& u)
        {
          float u0 = 0.0f;
          for (int i=0; i<20; i++) {
            const float fu = f(u0,d,p0,n0,r0,p1,n1,r1);
            const float dfu = dfds(u0,d,p0,n0,r0,p1,n1,r1);
            u0 = u0 - fu/dfu;
          }
          if (f(u0,d,p0,n0,r0,p1,n1,r1) > 0.01f) u0 = 2;
          const Vec3fa ps0 = (1.0f-u0)*p0 + u0*p1;
          const Vec3fa ns0 = (1.0f-u0)*n0 + u0*n1;
          float t0 = dot(ps0,ns0)/dot(d,ns0);
          if (u0 < 0.0f || u0 > 1.0f) t0 = inf;
          
          float u1 = 1.0f;
          for (int i=0; i<20; i++) {
            const float fu = f(u1,d,p0,n0,r0,p1,n1,r1);
            const float dfu = dfds(u1,d,p0,n0,r0,p1,n1,r1);
            u1 = u1 - fu/dfu;
          }
          if (f(u1,d,p0,n0,r0,p1,n1,r1) > 0.01f) u1 = 2;
          const Vec3fa ps1 = (1.0f-u1)*p0 + u1*p1;
          const Vec3fa ns1 = (1.0f-u1)*n0 + u1*n1;
          float t1 = dot(ps1,ns1)/dot(d,ns1);
          if (u1 < 0.0f || u1 > 1.0f) t1 = inf;

          if (t0 < t1) {
            u = u0;
          } else {
            u = u1;
          }
          
          return u >= 0.0f && u <= 1.0f;
        }

        template<typename Epilog>
        static __forceinline bool intersect(Ray& ray, const Precalculations& pre,
                                            const vbool<M>& valid_i, const Vec4vfM& v0, const Vec4vfM& v1, const Vec4vfM& v2, const Vec4vfM& v3, 
                                            const Epilog& epilog)
        {
          vbool<M> valid = valid_i;
#if 1
          const Vec3vfM Q1(v1.x,v1.y,v1.z);
          const Vec3vfM Q2(v2.x,v2.y,v2.z);
          valid &= abs(dot(Vec3vfM(ray.org)-Q1,normalize_safe(cross(Q2-Q1,Vec3vfM(ray.dir))))) <= max(v1.w,v2.w);
          if (none(valid)) return false;
#endif

#if 0
          /* transform end points into ray space */
          Vec4vfM p0(xfmVector(pre.ray_space,v1.xyz()-Vec3vfM(ray.org)), v1.w);
          Vec4vfM p1(xfmVector(pre.ray_space,v2.xyz()-Vec3vfM(ray.org)), v2.w);

          /* approximative intersection with cone */
          const Vec4vfM v = p1-p0;
          const Vec4vfM w = -p0;
          const vfloat<M> d0 = w.x*v.x + w.y*v.y;
          const vfloat<M> d1 = v.x*v.x + v.y*v.y;
          const vfloat<M> u = clamp(d0*rcp(d1),vfloat<M>(zero),vfloat<M>(one));
          const Vec4vfM p = p0 + u*v;
          const vfloat<M> t = p.z*pre.depth_scale;
          const vfloat<M> d2 = p.x*p.x + p.y*p.y;
          const vfloat<M> r = p.w;
          const vfloat<M> r2 = r*r;
          valid &= d2 <= r2 & vfloat<M>(ray.tnear) < t & t < vfloat<M>(ray.tfar);
          if (unlikely(none(valid))) return false;
          
          /* ignore denormalized segments */
          const Vec3vfM T = v2.xyz()-v1.xyz();
          valid &= T.x != vfloat<M>(zero) | T.y != vfloat<M>(zero) | T.z != vfloat<M>(zero);
          if (unlikely(none(valid))) return false;
          
          /* update hit information */
          LineIntersectorHitM<M> hit(u,zero,t,T);
          return epilog(valid,hit);
#endif

#if 0
          vfloat<M> t1,u1; Vec3<vfloat<M>> Ng1; 
          vbool<M> valid1 = intersect_cone(valid,ray,v1,v2,t1,u1,Ng1);
          valid1 &= u1 >= 0.0f & u1 <= 1.0f;
          if (none(valid1)) return false;
          LineIntersectorHitM<M> hit1(u1,zero,t1,Ng1);
          return epilog(valid1,hit1);
#endif

#if 0
          vfloat<M> t1,u1; Vec3<vfloat<M>> Ng1; 
          vbool<M> valid1 = intersect_cylinder(valid,ray,Vec3vfM(v1.x,v1.y,v1.z),Vec3vfM(v2.x,v2.y,v2.z),0.5f*(v0.w+v1.w),t1,u1,Ng1);
          valid1 &= u1 >= 0.0f & u1 <= 1.0f;
          if (none(valid1)) return false;
          LineIntersectorHitM<M> hit1(u1,zero,t1,Ng1);
          return epilog(valid1,hit1);
#endif

#if 0
          Vec3<vfloat<M>> q0(v0.x,v0.y,v0.z);
          Vec3<vfloat<M>> q1(v1.x,v1.y,v1.z);
          Vec3<vfloat<M>> q2(v2.x,v2.y,v2.z);
          Vec3<vfloat<M>> q3(v3.x,v3.y,v3.z);

          auto Hl = normalize_safe(q1-q0) + normalize_safe(q2-q1);
          auto Hr = normalize_safe(q1-q2) + normalize_safe(q2-q3);
          
          vfloat<M> t1,u1; Vec3<vfloat<M>> Ng1; vbool<M> valid1 = intersect_cone(valid,ray,v1,v2,t1,u1,Ng1);
          valid1 &= (ray.tnear < t1) & (t1 < ray.tfar);

          //if (unlikely(any(valid,v1.w != v2.w | u1*length(q2-q1) < v1.w)))
          {
            vfloat<M> tl,ul; Vec3<vfloat<M>> Ngl; vbool<M> validl = intersect_sphere(valid,ray,v1,tl,Ngl); ul = 0.0f;
            auto left = u1 < 0.0f;
            valid1 = select(left,validl,valid1);
            t1 = select(left, tl, t1);
            u1 = select(left, ul, u1);
            Ng1.x = select(left, Ngl.x, Ng1.x);
            Ng1.y = select(left, Ngl.y, Ng1.y);
            Ng1.z = select(left, Ngl.z, Ng1.z);
          }

          //if (unlikely(any(valid,v1.w != v2.w | (1.0f-u1)*length(q2-q1) < v1.w)))
          {
            vfloat<M> tr,ur; Vec3<vfloat<M>> Ngr; vbool<M> validr = intersect_sphere(valid,ray,v2,tr,Ngr); ur = 1.0f;
            auto right = u1 > 1.0f;
            valid1 = select(right,validr,valid1);
            t1 = select(right, tr, t1);
            u1 = select(right, ur, u1);
            Ng1.x = select(right, Ngr.x, Ng1.x);
            Ng1.y = select(right, Ngr.y, Ng1.y);
            Ng1.z = select(right, Ngr.z, Ng1.z);
          }

          auto thl = intersect_half_plane(ray,Hl,q1);
          valid1 &= thl.first <= t1 & t1 <= thl.second;

          auto thr = intersect_half_plane(ray,Hr,q2);
          valid1 &= thr.first <= t1 & t1 <= thr.second;
          
          if (none(valid1)) return false;
          LineIntersectorHitM<M> hit1(u1,zero,t1,Ng1);
          return epilog(valid1,hit1);

#endif

#if 0

          Vec3<vfloat<M>> q0(v0.x,v0.y,v0.z);
          Vec3<vfloat<M>> q1(v1.x,v1.y,v1.z);
          Vec3<vfloat<M>> q2(v2.x,v2.y,v2.z);
          Vec3<vfloat<M>> q3(v3.x,v3.y,v3.z);

          auto Hl = normalize_safe(q1-q0) + normalize_safe(q2-q1);
          auto Hr = normalize_safe(q1-q2) + normalize_safe(q2-q3);
          
          auto tpl0 = intersect_half_plane(ray,Hl,q1);
          auto tpl1 = intersect_half_plane(ray,q1-q2,q1);

          auto tpr0 = intersect_half_plane(ray,Hr,q2);
          auto tpr1 = intersect_half_plane(ray,q2-q1,q2);

          vfloat<M> tl,ul; Vec3<vfloat<M>> Ngl; vbool<M> validl = intersect_sphere(valid,ray,v1,tl,Ngl); ul = 0.0f;
          validl &= max(tpl0.first,tpl1.first) <= tl & tl <= min(tpl0.second,tpl1.second);

          //vfloat<M> t0,u0; Vec3<vfloat<M>> Ng0; vbool<M> valid0 = intersect_cone(valid,ray,v0,v1,t0,u0,Ng0);
          //valid0 &= (0.0f <= u0) & (u0 <= 1.0f);
          vfloat<M> t1,u1; Vec3<vfloat<M>> Ng1; vbool<M> valid1 = intersect_cone(valid,ray,v1,v2,t1,u1,Ng1);
          valid1 &= (0.0f <= u1) & (u1 <= 1.0f);
          //vfloat<M> t2,u2; Vec3<vfloat<M>> Ng2; vbool<M> valid2 = intersect_cone(valid,ray,v2,v3,t2,u2,Ng2);
          //valid2 &= (0.0f <= u2) & (u2 <= 1.0f);

          //valid1 &= !valid0 | (t0 > t1);
          //valid1 &= !valid2 | (t2 > t1);
          valid1 &= (ray.tnear < t1) & (t1 < ray.tfar);

          vfloat<M> tr,ur; Vec3<vfloat<M>> Ngr; vbool<M> validr = intersect_sphere(valid,ray,v2,tr,Ngr); ur = 1.0f;
          validr &= max(tpr0.first,tpr1.first) <= tr & tr <= min(tpr0.second,tpr1.second);

          //if (none(validr)) return false;
          //LineIntersectorHitM<M> hitr(ur,zero,tr,Ngr);
          //return epilog(validr,hitr);

          vbool<M> validk;
          vfloat<M> t,u; Vec3<vfloat<M>> Ng;
          validk = valid1; t = select(valid1,t1,float(inf)); u = u1; Ng = Ng1;

          vbool<M> va = !valid1 & validl & (tl < t);
          t = select(va, tl, t);
          vbool<M> vb = !valid1 & validr & (tr < t);
          t = select(vb, tr, t);

          u = select(va, ul, u);
          u = select(vb, ur, u);

          Ng.x = select(va, Ngl.x, Ng.x);
          Ng.x = select(vb, Ngr.x, Ng.x);

          Ng.y = select(va, Ngl.y, Ng.y);
          Ng.y = select(vb, Ngr.y, Ng.y);

          Ng.z = select(va, Ngl.z, Ng.z);
          Ng.z = select(vb, Ngr.z, Ng.z);

          validk = select(va, validl, validk);
          validk = select(vb, validr, validk);
          if (none(validk)) return false;
          
          LineIntersectorHitM<M> hit(u,zero,t,Ng);
          return epilog(validk,hit);

#endif

#if 0
          vbool<M> valid_o = false;
          LineIntersectorHitM<M> hit;
          
          for (size_t i=0; i<M; i++)
          {
            if (!valid[i]) continue;
            const Vec3fa p0(v0.x[i],v0.y[i],v0.z[i]);
            const Vec3fa p1(v1.x[i],v1.y[i],v1.z[i]);
            const Vec3fa p2(v2.x[i],v2.y[i],v2.z[i]);
            const Vec3fa p3(v3.x[i],v3.y[i],v3.z[i]);
            const Vec3fa n1 = normalize_safe(p1-p0) + normalize_safe(p2-p1);
            const Vec3fa n2 = normalize_safe(p2-p1) + normalize_safe(p3-p2);
            float u = 0.0f;
            if (!intersect_iterative1(ray.dir,p1-ray.org,n1,v1.w[i],p2-ray.org,n2,v2.w[i],u)) continue;
            const Vec3fa ps = (1.0f-u)*p1 + u*p2;
            const Vec3fa ns = (1.0f-u)*n1 + u*n2;
            const float t = dot(ps-ray.org,ns)/dot(ray.dir,ns);
            const Vec3fa os = ray.org+t*ray.dir;
            const Vec3fa Ng = os-ps;
            hit.vu[i] = u;
            hit.vv[i] = 0.0f;
            hit.vt[i] = t;
            hit.vNg.x[i] = Ng.x;
            hit.vNg.y[i] = Ng.y;
            hit.vNg.z[i] = Ng.z;
            set(valid_o,i);
          }
          if (none(valid_o)) return false;
          return epilog(valid_o,hit);

#endif

#if 1
          vbool<M> valid_o = false;
          LineIntersectorHitM<M> hit;

          const Vec3vfM vp0(v0.x,v0.y,v0.z);
          const Vec3vfM vp1(v1.x,v1.y,v1.z);
          const Vec3vfM vp2(v2.x,v2.y,v2.z);
          const Vec3vfM vp3(v3.x,v3.y,v3.z);
          const Vec3vfM vn1 = normalize_safe(vp1-vp0) + normalize_safe(vp2-vp1);
          const Vec3vfM vn2 = normalize_safe(vp2-vp1) + normalize_safe(vp3-vp2);
          
          for (size_t m=movemask(valid), i=__bsf(m); m!=0; m=__btc(m,i), i=__bsf(m))
          {
            //__asm("nop");
            const Vec3fa p1(vp1.x[i],vp1.y[i],vp1.z[i]);
            const Vec3fa p2(vp2.x[i],vp2.y[i],vp2.z[i]);
            const Vec3fa n1(vn1.x[i],vn1.y[i],vn1.z[i]);
            const Vec3fa n2(vn2.x[i],vn2.y[i],vn2.z[i]);
            /*PRINT(p1);
            PRINT(p2);
            PRINT(n1);
            PRINT(n2);*/
            //const Vec3fa n1(1,2,0);
            //const Vec3fa n2(1,0,2);
            float u = 0.0f;
            float t = 0.0f;
            Vec3fa Ng = zero;
            if (!intersect_fill_cone(ray,p1,n1,v1.w[i],p2,n2,v2.w[i],u,t,Ng)) continue;
            hit.vu[i] = u;
            hit.vv[i] = 0.0f;
            hit.vt[i] = t;
            hit.vNg.x[i] = Ng.x;
            hit.vNg.y[i] = Ng.y;
            hit.vNg.z[i] = Ng.z;
            set(valid_o,i);
          }
          if (none(valid_o)) return false;
          return epilog(valid_o,hit);
#endif
        }
      };
    
    template<int M, int K>
      struct LineIntersectorK
      {
        typedef Vec3<vfloat<M>> Vec3vfM;
        typedef Vec4<vfloat<M>> Vec4vfM;
        
        struct Precalculations 
        {
          __forceinline Precalculations (const vbool<K>& valid, const RayK<K>& ray)
          {
            int mask = movemask(valid);
            depth_scale = rsqrt(dot(ray.dir,ray.dir));
            while (mask) {
              size_t k = __bscf(mask);
              ray_space[k] = frame(depth_scale[k]*Vec3fa(ray.dir.x[k],ray.dir.y[k],ray.dir.z[k])).transposed();
            }
          }
          
          vfloat<K> depth_scale;
          LinearSpace3<Vec3vfM> ray_space[K];
        };
        
        template<typename Epilog>
        static __forceinline bool intersect(RayK<K>& ray, size_t k, const Precalculations& pre,
                                            const vbool<M>& valid_i, const Vec4vfM& v0, const Vec4vfM& v1,
                                            const Epilog& epilog)
        {
          /* transform end points into ray space */
          const Vec3vfM ray_org(ray.org.x[k],ray.org.y[k],ray.org.z[k]);
          const Vec3vfM ray_dir(ray.dir.x[k],ray.dir.y[k],ray.dir.z[k]);
          Vec4vfM p0(xfmVector(pre.ray_space[k],v0.xyz()-ray_org), v0.w);
          Vec4vfM p1(xfmVector(pre.ray_space[k],v1.xyz()-ray_org), v1.w);
          
          /* approximative intersection with cone */
          const Vec4vfM v = p1-p0;
          const Vec4vfM w = -p0;
          const vfloat<M> d0 = w.x*v.x + w.y*v.y;
          const vfloat<M> d1 = v.x*v.x + v.y*v.y;
          const vfloat<M> u = clamp(d0*rcp(d1),vfloat<M>(zero),vfloat<M>(one));
          const Vec4vfM p = p0 + u*v;
          const vfloat<M> t = p.z*pre.depth_scale[k];
          const vfloat<M> d2 = p.x*p.x + p.y*p.y;
          const vfloat<M> r = p.w;
          const vfloat<M> r2 = r*r;
          vbool<M> valid = valid_i & d2 <= r2 & vfloat<M>(ray.tnear[k]) < t & t < vfloat<M>(ray.tfar[k]);
          if (unlikely(none(valid))) return false;
          
          /* ignore denormalized segments */
          const Vec3vfM T = v1.xyz()-v0.xyz();
          valid &= T.x != vfloat<M>(zero) | T.y != vfloat<M>(zero) | T.z != vfloat<M>(zero);
          if (unlikely(none(valid))) return false;
          
          /* update hit information */
          LineIntersectorHitM<M> hit(u,zero,t,T);
          return epilog(valid,hit);
        }
      };
  }
}
