/******************************************************************************
 * $Id: osr_csharp.i f7564433d94dbf39984b1a961e148865b2c98262 2020-07-22 21:53:21 +0200 Tamas Szekeres $
 *
 * Name:     osr_csharp.i
 * Project:  GDAL CSharp Interface
 * Purpose:  OSR CSharp SWIG Interface declarations.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

%include cpl_exceptions.i

%include typemaps_csharp.i

%apply (int *hasval) {int* pnListCount};

%{
typedef OSRCRSInfo* OSRCRSInfoList;
%}

%typemap(cscode) OSRCRSInfoList %{  
  public CRSInfo this[int i]
  {
     get { return get(i); }
  }
%}


%rename (CRSInfoList) OSRCRSInfoList;

struct OSRCRSInfoList {
%extend {

  OSRCRSInfo* get(int index) {
     return self[index];
  }

  ~OSRCRSInfoList() {
    OSRDestroyCRSInfoList(self);
  }
} /* extend */
}; /* OSRCRSInfoList */

%newobject GetCRSInfoListFromDatabase;
%inline %{
OSRCRSInfoList* GetCRSInfoListFromDatabase( char* authName, int* pnListCount )
{
    return (OSRCRSInfoList*)OSRGetCRSInfoListFromDatabase(authName, NULL, pnListCount);
}
%}

