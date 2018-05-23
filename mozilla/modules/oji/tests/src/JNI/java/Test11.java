
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

public class Test11 extends Test2{

  static {
     System.loadLibrary("ojiapijnitests");
  
  }

  public void test(int i){
     mprint(i);
  }
  
  public native void mprint(int i);

  public static native void mprint_static(int i);
  
  public void jprint(int i){
      System.out.println("i="+i);
  }

  public static void jprint_static(int i){
      System.out.println("i="+i);
  }

  public native int Test1_method3_native(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]);

  public static native int Test1_method3_native_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]);

}

