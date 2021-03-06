/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      ETSI Plugtest resource
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <string.h>
#include "coap-engine.h"
#include "coap.h"
#include "random.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "Plugtest"
#define LOG_LEVEL LOG_LEVEL_PLUGTEST

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_delete_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_plugtest_test, "title=\"Default test resource\"", res_get_handler, res_post_handler, res_put_handler, res_delete_handler);

static uint8_t test_etag[8] = { 0 };
static uint8_t test_etag_len = 1;
static uint8_t test_change = 1;
static uint8_t test_none_match_okay = 1;

static const uint8_t *bytes = NULL;
static size_t len = 0;

static void
test_update_etag()
{
  int i;
  test_etag_len = (random_rand() % 8) + 1;
  for(i = 0; i < test_etag_len; ++i) {
    test_etag[i] = random_rand();
  }
  test_change = 0;

  LOG_DBG("### SERVER ACTION ### Changed ETag %u [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n", test_etag_len, test_etag[0], test_etag[1], test_etag[2], test_etag[3], test_etag[4], test_etag[5], test_etag[6], test_etag[7]);
}
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_message_t *const coap_req = (coap_message_t *)request;

  if(test_change) {
    test_update_etag();
  }
  LOG_DBG("/test GET (%s %u)\n", coap_req->type == COAP_TYPE_CON ? "CON" : "NON", coap_req->mid);

  if((len = coap_get_header_etag(request, &bytes)) > 0
     && len == test_etag_len
     && memcmp(test_etag, bytes, len) == 0) {
    LOG_DBG("validate\n");
    coap_set_status_code(response, VALID_2_03);
    coap_set_header_etag(response, test_etag, test_etag_len);

    test_change = 1;
    LOG_DBG("### SERVER ACTION ### Resource will change\n");
  } else {
    /* Code 2.05 CONTENT is default. */
    coap_set_header_content_format(response, TEXT_PLAIN);
    coap_set_header_etag(response, test_etag, test_etag_len);
    coap_set_header_max_age(response, 30);
    coap_set_payload(
      response,
      buffer,
      snprintf((char *)buffer, MAX_PLUGFEST_PAYLOAD, "Type: %u\nCode: %u\nMID: %u", coap_req->type, coap_req->code, coap_req->mid));
  }
}
static void
res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_message_t *const coap_req = (coap_message_t *)request;
  LOG_DBG("/test POST (%s %u)\n", coap_req->type == COAP_TYPE_CON ? "CON" : "NON", coap_req->mid);
  coap_set_status_code(response, CREATED_2_01);
  coap_set_header_location_path(response, "/location1/location2/location3");
}
static void
res_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_message_t *const coap_req = (coap_message_t *)request;
  LOG_DBG("/test PUT (%s %u)\n", coap_req->type == COAP_TYPE_CON ? "CON" : "NON", coap_req->mid);

  if(coap_get_header_if_none_match(request)) {
    if(test_none_match_okay) {
      coap_set_status_code(response, CREATED_2_01);

      test_none_match_okay = 0;
      LOG_DBG("### SERVER ACTION ### If-None-Match will FAIL\n");
    } else {
      coap_set_status_code(response, PRECONDITION_FAILED_4_12);

      test_none_match_okay = 1;
      LOG_DBG("### SERVER ACTION ### If-None-Match will SUCCEED\n");
    }
  } else if(((len = coap_get_header_if_match(request, &bytes)) > 0
             && (len == test_etag_len
                 && memcmp(test_etag, bytes, len) == 0))
            || len == 0) {
    test_update_etag();
    coap_set_header_etag(response, test_etag, test_etag_len);

    coap_set_status_code(response, CHANGED_2_04);

    if(len > 0) {
      test_change = 1;
      LOG_DBG("### SERVER ACTION ### Resource will change\n");
    }
  } else {
    LOG_DBG("Check %u/%u\n  [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n  [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",
           (unsigned)len,
           (unsigned)test_etag_len,
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
           test_etag[0], test_etag[1], test_etag[2], test_etag[3], test_etag[4], test_etag[5], test_etag[6], test_etag[7]);

    coap_set_status_code(response, PRECONDITION_FAILED_4_12);
  }
}
static void
res_delete_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_message_t *const coap_req = (coap_message_t *)request;
  LOG_DBG("/test DELETE (%s %u)\n", coap_req->type == COAP_TYPE_CON ? "CON" : "NON", coap_req->mid);
  coap_set_status_code(response, DELETED_2_02);
}
