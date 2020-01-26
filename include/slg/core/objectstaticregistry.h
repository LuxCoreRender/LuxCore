/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_OBJECTSTATICREGISTRY_H
#define	_SLG_OBJECTSTATICREGISTRY_H

#include <string>
#include <vector>

#include "slg/core/statictable.h"

namespace slg {

//------------------------------------------------------------------------------
// ObjectStaticRegistry
//------------------------------------------------------------------------------

// For an easy the declaration and registration of each Object sub-class
// with Object StaticTable
#define OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(R, C) \
STATICTABLE_DECLARE_REGISTRATION(R, C, std::string, GetObjectType); \
STATICTABLE_DECLARE_REGISTRATION(R, C, ObjectType, GetObjectTag); \
STATICTABLE_DECLARE_REGISTRATION(R, C, std::string, ToProperties); \
STATICTABLE_DECLARE_REGISTRATION(R, C, std::string, FromProperties); \
STATICTABLE_DECLARE_REGISTRATION(R, C, std::string, FromPropertiesOCL)

#define OBJECTSTATICREGISTRY_REGISTER(R, C) \
STATICTABLE_REGISTER(R, C, C::GetObjectTag(), std::string, GetObjectType); \
STATICTABLE_REGISTER(R, C, C::GetObjectType(), R::ObjectType, GetObjectTag); \
STATICTABLE_REGISTER(R, C, C::GetObjectTag(), std::string, ToProperties); \
STATICTABLE_REGISTER(R, C, C::GetObjectTag(), std::string, FromProperties); \
STATICTABLE_REGISTER(R, C, C::GetObjectTag(), std::string, FromPropertiesOCL)

// For an easy declaration of ObjectStaticRegistry like SamplerRegistry, etc.
#define OBJECTSTATICREGISTRY_DECLARE_STATICFIELDS(R) \
STATICTABLE_DECLARE_DECLARATION(R, std::string, GetObjectType); \
STATICTABLE_DECLARE_DECLARATION(R, ObjectType, GetObjectTag); \
STATICTABLE_DECLARE_DECLARATION(R, std::string, ToProperties); \
STATICTABLE_DECLARE_DECLARATION(R, std::string, FromProperties); \
STATICTABLE_DECLARE_DECLARATION(R, std::string, FromPropertiesOCL)

#define OBJECTSTATICREGISTRY_STATICFIELDS(R) \
STATICTABLE_DECLARATION(R, std::string, GetObjectType); \
STATICTABLE_DECLARATION(R, R::ObjectType, GetObjectTag); \
STATICTABLE_DECLARATION(R, std::string, ToProperties); \
STATICTABLE_DECLARATION(R, std::string, FromProperties); \
STATICTABLE_DECLARATION(R, std::string, FromPropertiesOCL)
}

#endif	/* _SLG_OBJECTSTATICREGISTRY_H */
