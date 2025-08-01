/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include "first.h"

#define NUM_THREADS 100

#ifdef _WIN32
#if defined(CURL_WINDOWS_UWP) || defined(UNDER_CE)
static DWORD WINAPI t3026_run_thread(LPVOID ptr)
#else
#include <process.h>
static unsigned int WINAPI t3026_run_thread(void *ptr)
#endif
{
  CURLcode *result = ptr;

  *result = curl_global_init(CURL_GLOBAL_ALL);
  if(*result == CURLE_OK)
    curl_global_cleanup();

  return 0;
}

static CURLcode test_lib3026(char *URL)
{
#if defined(CURL_WINDOWS_UWP) || defined(UNDER_CE)
  typedef HANDLE curl_win_thread_handle_t;
#else
  typedef uintptr_t curl_win_thread_handle_t;
#endif
  CURLcode results[NUM_THREADS];
  curl_win_thread_handle_t ths[NUM_THREADS];
  unsigned tid_count = NUM_THREADS, i;
  CURLcode test_failure = CURLE_OK;
  curl_version_info_data *ver;
  (void) URL;

  ver = curl_version_info(CURLVERSION_NOW);
  if((ver->features & CURL_VERSION_THREADSAFE) == 0) {
    curl_mfprintf(stderr, "%s:%d On Windows but the "
                  "CURL_VERSION_THREADSAFE feature flag is not set\n",
                  __FILE__, __LINE__);
    return TEST_ERR_MAJOR_BAD;
  }

  for(i = 0; i < tid_count; i++) {
    curl_win_thread_handle_t th;
    results[i] = CURL_LAST; /* initialize with invalid value */
#if defined(CURL_WINDOWS_UWP) || defined(UNDER_CE)
    th = CreateThread(NULL, 0, t3026_run_thread, &results[i], 0, NULL);
#else
    th = _beginthreadex(NULL, 0, t3026_run_thread, &results[i], 0, NULL);
#endif
    if(!th) {
      curl_mfprintf(stderr, "%s:%d Couldn't create thread, errno %lu\n",
                    __FILE__, __LINE__, GetLastError());
      tid_count = i;
      test_failure = TEST_ERR_MAJOR_BAD;
      goto cleanup;
    }
    ths[i] = th;
  }

cleanup:
  for(i = 0; i < tid_count; i++) {
    WaitForSingleObject((HANDLE)ths[i], INFINITE);
    CloseHandle((HANDLE)ths[i]);
    if(results[i] != CURLE_OK) {
      curl_mfprintf(stderr, "%s:%d thread[%u]: curl_global_init() failed,"
                    "with code %d (%s)\n", __FILE__, __LINE__,
                    i, (int) results[i], curl_easy_strerror(results[i]));
      test_failure = TEST_ERR_MAJOR_BAD;
    }
  }

  return test_failure;
}

#elif defined(HAVE_PTHREAD_H)
#include <pthread.h>

static void *t3026_run_thread(void *ptr)
{
  CURLcode *result = ptr;

  *result = curl_global_init(CURL_GLOBAL_ALL);
  if(*result == CURLE_OK)
    curl_global_cleanup();

  return NULL;
}

static CURLcode test_lib3026(char *URL)
{
  CURLcode results[NUM_THREADS];
  pthread_t tids[NUM_THREADS];
  unsigned tid_count = NUM_THREADS, i;
  CURLcode test_failure = CURLE_OK;
  curl_version_info_data *ver;
  (void) URL;

  ver = curl_version_info(CURLVERSION_NOW);
  if((ver->features & CURL_VERSION_THREADSAFE) == 0) {
    curl_mfprintf(stderr, "%s:%d Have pthread but the "
                  "CURL_VERSION_THREADSAFE feature flag is not set\n",
                  __FILE__, __LINE__);
    return TEST_ERR_MAJOR_BAD;
  }

  for(i = 0; i < tid_count; i++) {
    int res;
    results[i] = CURL_LAST; /* initialize with invalid value */
    res = pthread_create(&tids[i], NULL, t3026_run_thread, &results[i]);
    if(res) {
      curl_mfprintf(stderr, "%s:%d Couldn't create thread, errno %d\n",
                    __FILE__, __LINE__, res);
      tid_count = i;
      test_failure = TEST_ERR_MAJOR_BAD;
      goto cleanup;
    }
  }

cleanup:
  for(i = 0; i < tid_count; i++) {
    pthread_join(tids[i], NULL);
    if(results[i] != CURLE_OK) {
      curl_mfprintf(stderr, "%s:%d thread[%u]: curl_global_init() failed,"
                    "with code %d (%s)\n", __FILE__, __LINE__,
                    i, (int) results[i], curl_easy_strerror(results[i]));
      test_failure = TEST_ERR_MAJOR_BAD;
    }
  }

  return test_failure;
}

#else /* without pthread or Windows, this test doesn't work */
static CURLcode test_lib3026(char *URL)
{
  curl_version_info_data *ver;
  (void)URL;

  ver = curl_version_info(CURLVERSION_NOW);
  if((ver->features & CURL_VERSION_THREADSAFE) != 0) {
    curl_mfprintf(stderr, "%s:%d No pthread but the "
                  "CURL_VERSION_THREADSAFE feature flag is set\n",
                  __FILE__, __LINE__);
    return TEST_ERR_MAJOR_BAD;
  }
  return CURLE_OK;
}
#endif
