/*
  Copyright (C) 2005-2010 Steven L. Scott

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#ifndef BOOM_SUFSTAT_ABSTRACT_COMBINE_IMPL_HPP_
#define BOOM_SUFSTAT_ABSTRACT_COMBINE_IMPL_HPP_

#include <Models/Sufstat.hpp>
#include <cpputil/report_error.hpp>

// NOTE:  this file must not be included in another header file.
namespace BOOM{

  template <class ConcreteSuf>
  ConcreteSuf * abstract_combine_impl(ConcreteSuf *me, Sufstat *s){
    ConcreteSuf * cs = dynamic_cast<ConcreteSuf *>(s);
    if(!cs){
      report_error("Cannot cast Sufstat to concrete type");
    }
    me->combine(*cs);
    return me;
  }

}
#endif// BOOM_SUFSTAT_ABSTRACT_COMBINE_IMPL_HPP_
