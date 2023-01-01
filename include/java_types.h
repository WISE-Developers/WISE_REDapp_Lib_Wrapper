/**
 * WISE_REDapp_Lib_Wrapper: java_types.h
 * Copyright (C) 2023  WISE
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>


#define JTypeInt "I"
#define JTypeShort "S"
#define JTypeLong "J"
#define JTypeFloat "F"
#define JTypeDouble "D"
#define JTypeBool "Z"
#define JTypeByte "B"
#define JTypeChar "C"
#define JTypeVoid "V"

/**
 * Creates a Java class name.
 * 
 * ex. JTypeJavaClass(String) provides "java/lang/String".
 */
#define JTypeJavaClass(x) "java/lang/" #x
#define JTypeString JTypeJavaClass(String)
#define JTypeObject JTypeJavaClass(Object)

/**
 * Creates an array type for primitive types.
 * 
 * ex. JTypeArray(JTypeInt) provides "[I" which translates to int[].
 */
#define JTypeArray(x) "[" x

/**
 * Creates an array type for objects.
 * 
 * ex. JTypeArray(JTypeString) provides "[Ljava/lang/String;" which translates to String[].
 */
#define JTypeOjbectArray(x) "[L" x ";"

/**
 * Create a method parameter from a class type.
 * 
 * ex. JObjectParameter(java/lang/String) returns "Ljava/lang/String;" which translates to String.
 */
#define JObjectParameter(type) "L" #type ";"
#define JParameter(type) "L" type ";"

#define JMethodDefinition(params, retval) "(" params ")" retval
