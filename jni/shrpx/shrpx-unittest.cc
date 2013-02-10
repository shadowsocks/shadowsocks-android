/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdio.h>
#include <string.h>
#include <CUnit/Basic.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
/* include test cases' include files here */
#include "shrpx_ssl_test.h"

static int init_suite1(void)
{
  return 0;
}

static int clean_suite1(void)
{
  return 0;
}


int main(int argc, char* argv[])
{
   CU_pSuite pSuite = NULL;
   unsigned int num_tests_failed;

   OpenSSL_add_all_algorithms();
   SSL_load_error_strings();
   SSL_library_init();

   /* initialize the CUnit test registry */
   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   /* add a suite to the registry */
   pSuite = CU_add_suite("shrpx_TestSuite", init_suite1, clean_suite1);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   if(!CU_add_test(pSuite, "ssl_create_lookup_tree",
                   shrpx::test_shrpx_ssl_create_lookup_tree) ||
      !CU_add_test(pSuite, "ssl_cert_lookup_tree_add_cert_from_file",
                   shrpx::test_shrpx_ssl_cert_lookup_tree_add_cert_from_file)) {
     CU_cleanup_registry();
     return CU_get_error();
   }

   /* Run all tests using the CUnit Basic interface */
   CU_basic_set_mode(CU_BRM_VERBOSE);
   CU_basic_run_tests();
   num_tests_failed = CU_get_number_of_tests_failed();
   CU_cleanup_registry();
   if(CU_get_error() == CUE_SUCCESS) {
     return num_tests_failed;
   } else {
     printf("CUnit Error: %s\n", CU_get_error_msg());
     return CU_get_error();
   }
}
