/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2020, Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "test.h"

#include "rdkafka.h"

#include "../src/rdkafka_proto.h"
#include "../src/rdunittest.h"

#include <stdarg.h>


/**
 * @name Misc mock-injected errors.
 *
 */

/**
 * @brief Test producer handling (retry) of ERR_KAFKA_STORAGE_ERROR.
 */
static void do_test_producer_storage_error (rd_bool_t too_few_retries) {
        rd_kafka_conf_t *conf;
        rd_kafka_t *rk;
        rd_kafka_mock_cluster_t *mcluster;
        rd_kafka_resp_err_t err;

        TEST_SAY(_C_MAG "[ %s%s ]\n", __FUNCTION__,
                 too_few_retries ? ": with too few retries" : "");

        test_conf_init(&conf, NULL, 10);

        test_conf_set(conf, "test.mock.num.brokers", "3");
        test_conf_set(conf, "retries", too_few_retries ? "1" : "10");
        test_conf_set(conf, "retry.backoff.ms", "500");
        rd_kafka_conf_set_dr_msg_cb(conf, test_dr_msg_cb);

        test_curr->ignore_dr_err = rd_false;
        if (too_few_retries) {
                test_curr->exp_dr_err = RD_KAFKA_RESP_ERR_KAFKA_STORAGE_ERROR;
                test_curr->exp_dr_status = RD_KAFKA_MSG_STATUS_NOT_PERSISTED;
        } else {
                test_curr->exp_dr_err = RD_KAFKA_RESP_ERR_NO_ERROR;
                test_curr->exp_dr_status = RD_KAFKA_MSG_STATUS_PERSISTED;
        }

        rk = test_create_handle(RD_KAFKA_PRODUCER, conf);

        mcluster = rd_kafka_handle_mock_cluster(rk);
        TEST_ASSERT(mcluster, "missing mock cluster");

        rd_kafka_mock_push_request_errors(
                mcluster,
                RD_KAFKAP_Produce,
                3,
                RD_KAFKA_RESP_ERR_KAFKA_STORAGE_ERROR,
                RD_KAFKA_RESP_ERR_KAFKA_STORAGE_ERROR,
                RD_KAFKA_RESP_ERR_KAFKA_STORAGE_ERROR);

        err = rd_kafka_producev(rk,
                                RD_KAFKA_V_TOPIC("mytopic"),
                                RD_KAFKA_V_VALUE("hi", 2),
                                RD_KAFKA_V_END);
        TEST_ASSERT(!err, "produce failed: %s", rd_kafka_err2str(err));

        /* Wait for delivery report. */
        test_flush(rk, 5000);

        rd_kafka_destroy(rk);

        TEST_SAY(_C_GRN "[ %s%s PASS ]\n", __FUNCTION__,
                 too_few_retries ? ": with too few retries" : "");
}


int main_0117_mock_errors (int argc, char **argv) {
        if (test_needs_auth()) {
                TEST_SKIP("Mock cluster does not support SSL/SASL\n");
                return 0;
        }

        do_test_producer_storage_error(rd_false);
        do_test_producer_storage_error(rd_true);

        return 0;
}
