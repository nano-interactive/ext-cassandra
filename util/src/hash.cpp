/**
 * Copyright 2015-2017 DataStax, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <php_driver.h>
#include <php_driver_types.h>

#include <util/hash.h>

static inline cass_int64_t
double_to_bits(cass_double_t value)
{
  cass_int64_t bits;
  if (zend_isnan(value))
    return 0x7ff8000000000000LL; /* A canonical NaN value */
  memcpy(&bits, &value, sizeof(cass_int64_t));
  return bits;
}

static inline unsigned
double_hash(cass_double_t value)
{
  return php_driver_bigint_hash(double_to_bits(value));
}

uint32_t
php_driver_value_hash(zval* zvalue)
{
  switch (Z_TYPE_P(zvalue)) {
  case IS_LONG:
    return php_driver_bigint_hash(Z_LVAL_P(zvalue));
  case IS_DOUBLE:
    return double_hash(Z_DVAL_P(zvalue));
  case IS_TRUE:
    return 1;
  case IS_FALSE:
    return 0;
  case IS_STRING:
    return zend_inline_hash_func(Z_STRVAL_P(zvalue), Z_STRLEN_P(zvalue));
  case IS_OBJECT:
    return ((php_driver_value_handlers*) Z_OBJ_P(zvalue)->handlers)->hash_value(zvalue);
  default:
    return 0;
  }
}

static inline int32_t
double_compare(cass_double_t d1, cass_double_t d2)
{
  cass_int64_t bits1, bits2;
  if (d1 < d2)
    return -1;
  if (d1 > d2)
    return 1;
  bits1 = double_to_bits(d1);
  bits2 = double_to_bits(d2);
  /* Handle NaNs and negative and positive 0.0 */
  return PHP_DRIVER_COMPARE(bits1, bits2);
}

int
php_driver_value_compare(zval* zvalue1, zval* zvalue2)
{
  if (zvalue1 == zvalue2)
    return 0;

  if (Z_TYPE_P(zvalue1) != Z_TYPE_P(zvalue2)) {
    return Z_TYPE_P(zvalue1) < Z_TYPE_P(zvalue2) ? -1 : 1;
  }

  switch (Z_TYPE_P(zvalue1)) {
  case IS_NULL:
    return 0;

  case IS_LONG:
    return PHP_DRIVER_COMPARE(Z_LVAL_P(zvalue1), Z_LVAL_P(zvalue2));

  case IS_DOUBLE:
    return double_compare(Z_DVAL_P(zvalue1), Z_DVAL_P(zvalue2));

  case IS_TRUE:
    return Z_TYPE_P(zvalue2) == IS_TRUE ? 0 : 1;

  case IS_FALSE:
    return Z_TYPE_P(zvalue2) == IS_FALSE ? 0 : -1;
  case IS_STRING:
    return zend_binary_zval_strcmp(zvalue1, zvalue2);
  case IS_OBJECT:
    return Z_OBJ_P(zvalue1)->handlers->compare(zvalue1, zvalue2);
  default:
    return 1;
  }
}

int
php_driver_data_compare(Bucket* a, Bucket* b)
{
  zval* first  = &a->val;
  zval* second = &b->val;

  return php_driver_value_compare(first, second);
}

uint32_t
php_driver_mpz_hash(uint32_t seed, mpz_t n)
{
  mp_size_t i;
  mp_size_t size = mpz_size(n);
  unsigned hashv = seed;
#if GMP_LIMB_BITS == 32
  for (i = 0; i < size; ++i) {
    hashv = php_driver_combine_hash(hashv, mpz_getlimbn(n, i));
  }
#elif GMP_LIMB_BITS == 64
  for (i = 0; i < size; ++i) {
    hashv = php_driver_combine_hash(hashv, php_driver_bigint_hash(mpz_getlimbn(n, i)));
  }
#else
#error "Unexpected GMP limb bits size"
#endif
  return hashv;
}
