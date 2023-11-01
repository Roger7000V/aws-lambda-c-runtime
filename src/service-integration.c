/*
 * Copyright (2019) - Paulo Miguel Almeida
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aws-lambda/c-runtime/utils.h"
#include "aws-lambda/http/service-integration.h"
#include "aws-lambda/http/response.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define HTTP_HEADER_CONTENT_TYPE "Content-Type: "
#define HTTP_HEADER_CONTENT_LENGTH "Content-Length: "

static CURL *curl;
static char base_url[256];
static char *next_endpoint, *result_endpoint;

/* Prototypes */
static inline char *build_url(char *b_url, char *path);

void service_integration_init(void) {
    strcpy(base_url, "http://");
    char *env;
    if ((env = getenv("AWS_LAMBDA_RUNTIME_API"))) {
        printf("LAMBDA_SERVER_ADDRESS defined in environment as: %s\n", env);
        strcat(base_url, env);
    }
    next_endpoint = build_url(base_url, "/2018-06-01/runtime/invocation/next");
    result_endpoint = build_url(base_url, "/2018-06-01/runtime/invocation/");

    curl_global_init(CURL_GLOBAL_ALL);
    http_response_init();
}

void service_integration_cleanup(void) {
    curl_global_cleanup();
    SAFE_FREE(next_endpoint);
    SAFE_FREE(result_endpoint);
    http_response_cleanup();
}

void set_default_curl_options(void) {
    // lambda freezes the container when no further tasks are available. The freezing period could be longer than the
    // request timeout, which causes the following get_next request to fail with a timeout error.
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    /* enable if you want to debug */
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
}

void set_curl_next_options(void) {
    set_default_curl_options();
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, next_endpoint);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback);
}

void set_curl_post_result_options(void) {
    set_default_curl_options();
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_data_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback);
}

next_outcome request_get_next(void) {
    // initialise struct with sentinel values
    next_outcome ret;
    ret.success = false;
    ret.res_code = -1; // REQUEST_NOT_MADE
    ret.request = NULL;

    http_response_clear();
    curl = curl_easy_init();
    if (curl) {
        set_curl_next_options();
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, get_user_agent_header());
        printf("Making request to %s\n", next_endpoint);
        CURLcode curl_code = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        if (curl_code != CURLE_OK) {
            printf("CURL returned error code %d - %s\n", curl_code, curl_easy_strerror(curl_code));
            printf("Failed to get next invocation. No Response from endpoint\n");
        } else {
            printf("1\n");
            printf("CURL response body: %s\n", http_response_get_content());
            ret.success = true;
            printf("2\n");
            ret.request = malloc(sizeof(invocation_request));
            printf("3\n");
            FAIL_IF(!ret.request)
            printf("4\n");
            ret.request->payload = NULL;
            printf("5\n");
            ret.request->request_id = NULL;
            printf("6\n");
            ret.request->xray_trace_id = NULL;
            printf("7\n");
            ret.request->client_context = NULL;
            printf("8\n");
            ret.request->cognito_identity = NULL;
            printf("9\n");
            ret.request->function_arn = NULL;
            printf("10\n");

            long resp_code;
            printf("11\n");
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp_code);
            printf("12\n");
            ret.res_code = (int) resp_code;
            printf("13\n");

            SAFE_STRDUP(ret.request->payload, http_response_get_content())
            printf("14\n");
            if (has_header(REQUEST_ID_HEADER)) {
                printf("15\n");
                SAFE_STRDUP(ret.request->request_id, get_header(REQUEST_ID_HEADER))
                printf("16\n");
                printf("ret.request.request_id: %s\n", ret.request->request_id);
            }

            if (has_header(TRACE_ID_HEADER)) {
                printf("17\n");
                SAFE_STRDUP(ret.request->xray_trace_id, get_header(TRACE_ID_HEADER))
                printf("18\n");
                printf("ret.request.xray_trace_id: %s\n", ret.request->xray_trace_id);
            }

            if (has_header(CLIENT_CONTEXT_HEADER)) {
                printf("19\n");
                SAFE_STRDUP(ret.request->client_context, get_header(CLIENT_CONTEXT_HEADER))
                printf("20\n");
                printf("ret.request.client_context: %s\n", ret.request->client_context);
            }

            if (has_header(COGNITO_IDENTITY_HEADER)) {
                printf("21\n");
                SAFE_STRDUP(ret.request->cognito_identity, get_header(COGNITO_IDENTITY_HEADER))
                printf("22\n");
                printf("ret.request.cognito_identity: %s\n", ret.request->cognito_identity);
            }

            if (has_header(FUNCTION_ARN_HEADER)) {
                printf("23\n");
                SAFE_STRDUP(ret.request->function_arn, get_header(FUNCTION_ARN_HEADER))
                printf("24\n");
                printf("ret.request.function_arn: %s\n", ret.request->function_arn);
            }

            //TODO add/create handler for deadline-ms-handler
//            if (resp.has_header(DEADLINE_MS_HEADER)) {
//                auto const& deadline_string = resp.get_header(DEADLINE_MS_HEADER);
//                unsigned long ms = strtoul(deadline_string.c_str(), nullptr, 10);
//                assert(ms > 0);
//                assert(ms < ULONG_MAX);
//                req.deadline += std::chrono::milliseconds(ms);
//                logging::log_info(
//                        LOG_TAG,
//                        "Received payload: %s\nTime remaining: %" PRId64,
//                        req.payload.c_str(),
//                        static_cast<int64_t>(req.get_time_remaining().count()));
//            }
        }
        printf("25\n");
        curl_easy_cleanup(curl);
        printf("26\n");
    }
    return ret;
}

post_result_outcome request_post_result(invocation_request *request, invocation_response *response) {
    printf("27\n");
    post_result_outcome ret;
    printf("28\n");
    ret.success = false;
    printf("29\n");
    ret.res_code = -1; // REQUEST_NOT_MADE
    printf("30\n");

    http_response_clear();
    printf("31\n");
    curl = curl_easy_init();
    printf("32\n");
    if (curl) {
        printf("33\n");
        // this ought to be enough space to accommodate request_id and the last url segment (including \0).
        char *request_url = malloc(strlen(result_endpoint) + 128);
        printf("34\n");
        FAIL_IF(!request_url)
        printf("35\n");
        strcpy(request_url, result_endpoint);
        printf("36\n");
        strcat(request_url, request->request_id);
        printf("37\n");
        strcat(request_url, (response->success) ? "/response" : "/error");
        printf("38\n");
        printf("Making request to %s\n", request_url);

        set_curl_post_result_options();
        printf("39\n");
        curl_easy_setopt(curl, CURLOPT_URL, request_url);
        printf("40\n");
        struct curl_slist *headers = NULL;
        printf("41\n");

        char *content_type_h = malloc(strlen(HTTP_HEADER_CONTENT_TYPE) + strlen(response->content_type) + 1);
        printf("42\n");
        FAIL_IF(!content_type_h)
        printf("43\n");
        strcpy(content_type_h, HTTP_HEADER_CONTENT_TYPE);
        printf("44\n");
        strcat(content_type_h, response->content_type);
        printf("45\n");
        printf("content_type_h -> %s\n", content_type_h);

        printf("46\n");
        headers = curl_slist_append(headers, content_type_h);
        printf("47\n");
        headers = curl_slist_append(headers, get_user_agent_header());
        printf("48\n");

        printf("calculating content length... %lu bytes\n", strlen(response->payload));
        char *content_length_v = malloc(16); //Lambda payload in bytes -> 7 digits + 1 null char + future proof fat
        printf("49\n");
        FAIL_IF(!content_length_v)
        printf("50\n");
        sprintf(content_length_v, "%lu", strlen(response->payload));
        printf("51\n");

        char *content_length_h = malloc(strlen(HTTP_HEADER_CONTENT_LENGTH) + strlen(content_length_v) + 1);
        printf("52\n");
        FAIL_IF(!content_length_h)
        printf("53\n");
        sprintf(content_length_h, "%s%s", HTTP_HEADER_CONTENT_LENGTH, content_length_v);
        printf("54\n");

        headers = curl_slist_append(headers, content_length_h);
        printf("55\n");

        curl_request_write_t req_write;
        printf("56\n");
        req_write.readptr = response->payload;
        printf("57\n");
        req_write.sizeleft = strlen(response->payload);
        printf("58\n");

        curl_easy_setopt(curl, CURLOPT_READDATA, &req_write);
        printf("59\n");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req_write.sizeleft);
        printf("60\n");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        printf("61\n");
        CURLcode curl_code = curl_easy_perform(curl);
        printf("62\n");
        curl_slist_free_all(headers);
        printf("63\n");

        if (curl_code != CURLE_OK) {
            printf("64\n");
            printf("CURL returned error code %d - %s\n", curl_code, curl_easy_strerror(curl_code));
        } else {
            printf("65\n");
            printf("CURL response body: %s\n", http_response_get_content());

            long http_response_code;
            printf("66\n");
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
            printf("67\n");
            ret.res_code = (int) http_response_code;
            printf("68\n");
            ret.success = (http_response_code >= 200 && http_response_code <= 299);
            printf("69\n");
        }

        printf("70\n");
        curl_easy_cleanup(curl);
        printf("71\n");
        printf("Cleaned UP\n");
        SAFE_FREE(request_url);
        printf("72\n");
        SAFE_FREE(content_type_h);
        printf("73\n");
        SAFE_FREE(content_length_h);
        printf("74\n");
        SAFE_FREE(content_length_v);
        printf("75\n");
    }
    return ret;
}

/* Utility functions */
static inline char *build_url(char *b_url, char *path) {
    printf("76\n");
    char *dest = malloc(strlen(b_url) + strlen(path) + 1);
    printf("77\n");
    FAIL_IF(!dest)
    printf("78\n");
    strcpy(dest, b_url);
    printf("79\n");
    strcat(dest, path);
    printf("80\n");
    return dest;
}
