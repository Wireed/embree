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

#include "triangle4v_mb.h"

namespace embree
{
  Triangle4vMB::Type Triangle4vMB::type;

  Triangle4vMB::Type::Type () 
  : PrimitiveType("triangle4vmb",sizeof(Triangle4vMB),4,false,1) {} 
  
  size_t Triangle4vMB::Type::blocks(size_t x) const {
    return (x+3)/4;
  }
  
  size_t Triangle4vMB::Type::size(const char* This) const {
    return ((Triangle4vMB*)This)->size();
  }
}
