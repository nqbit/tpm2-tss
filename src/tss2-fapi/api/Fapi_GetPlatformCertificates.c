/* SPDX-License-Identifier: BSD-2-Clause */
/*******************************************************************************
 * Copyright 2018-2019, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 ******************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "tss2_fapi.h"
#include "fapi_int.h"
#include "fapi_util.h"
#include "tss2_esys.h"
#define LOGMODULE fapi
#include "util/log.h"
#include "util/aux_util.h"

/** One-Call function for Fapi_GetPlatformCertificates
 *
 * Platform certificates for TPM 2.0 can consist not only of a single certificate
 * but a series of so-called delta certificates.
 * This function returns the set of Platform certificates concatenated in
 * a continuous buffer.
 *
 * @param [in,out] context The FAPI_CONTEXT
 * @param [out] certificates The platform certificates
 * @param [out] certificatesSize The size of the buffer with the certificates.
 *              May be NULL
 *
 * @retval TSS2_RC_SUCCESS: if the function call was a success.
 * @retval TSS2_FAPI_RC_BAD_REFERENCE: if context or certificates is NULL.
 * @retval TSS2_FAPI_RC_BAD_CONTEXT: if context corruption is detected.
 * @retval TSS2_FAPI_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_FAPI_RC_IO_ERROR: if the data cannot be saved.
 * @retval TSS2_FAPI_RC_MEMORY: if the FAPI cannot allocate enough memory for internal
 *         operations or return parameters.
 */
TSS2_RC
Fapi_GetPlatformCertificates(
    FAPI_CONTEXT *context,
    uint8_t **certificates,
    size_t *certificatesSize)
{
    LOG_TRACE("called for context:%p", context);

    TSS2_RC r, r2;

    /* Check for NULL parameters */
    check_not_null(context);
    check_not_null(certificates);

    /* Check whether TCTI and ESYS are initialized */
    return_if_null(context->esys, "Command can't be executed in none TPM mode.",
                   TSS2_FAPI_RC_NO_TPM);

    /* If the async state automata of FAPI shall be tested, then we must not set
       the timeouts of ESYS to blocking mode.
       During testing, the mssim tcti will ensure multiple re-invocations.
       Usually however the synchronous invocations of FAPI shall instruct ESYS
       to block until a result is available. */
#ifndef TEST_FAPI_ASYNC
    r = Esys_SetTimeout(context->esys, TSS2_TCTI_TIMEOUT_BLOCK);
    return_if_error_reset_state(r, "Set Timeout to blocking");
#endif /* TEST_FAPI_ASYNC */

    r = Fapi_GetPlatformCertificates_Async(context);
    return_if_error_reset_state(r, "Path_PlatformGetCertificate");

    do {
        /* We wait for file I/O to be ready if the FAPI state automata
           are in a file I/O state. */
        r = ifapi_io_poll(&context->io);
        return_if_error(r, "Something went wrong with IO polling");

        /* Repeatedly call the finish function, until FAPI has transitioned
           through all execution stages / states of this invocation. */
        r = Fapi_GetPlatformCertificates_Finish(context, certificates,
                certificatesSize);
    } while ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN);

    /* Reset the ESYS timeout to non-blocking, immediate response. */
    r2 = Esys_SetTimeout(context->esys, 0);
    return_if_error(r2, "Set Timeout to non-blocking");

    return_if_error_reset_state(r, "Path_PlatformGetCertificate");

    LOG_TRACE("finsihed");
    return TSS2_RC_SUCCESS;
}

/** Asynchronous function for Fapi_GetPlatformCertificates
 *
 * Platform certificates for TPM 2.0 can consist not only of a single certificate
 * but a series of so-called delta certificates.
 * This function returns the set of Platform certificates concatenated in
 * a continuous buffer.
 *
 * Call Fapi_GetPlatformCertificates_Finish to finish the execution of this
 * command.
 *
 * @param [in,out] context The FAPI_CONTEXT
 *
 * @retval TSS2_RC_SUCCESS: if the function call was a success.
 * @retval TSS2_FAPI_RC_BAD_REFERENCE: if context is NULL.
 * @retval TSS2_FAPI_RC_BAD_CONTEXT: if context corruption is detected.
 * @retval TSS2_FAPI_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_FAPI_RC_IO_ERROR: if the data cannot be saved.
 * @retval TSS2_FAPI_RC_MEMORY: if the FAPI cannot allocate enough memory for internal
 *         operations or return parameters.
 */
TSS2_RC
Fapi_GetPlatformCertificates_Async(
    FAPI_CONTEXT *context)
{
    LOG_TRACE("called for context:%p", context);

    TSS2_RC r;

    /* Check for NULL parameters */
    check_not_null(context);

    r = ifapi_session_init(context);
    return_if_error(r, "Initialize Fapi_GetPlatformCertificates");

    context->state = GET_PLATFORM_CERTIFICATE;

    LOG_TRACE("finsihed");
    return TSS2_RC_SUCCESS;
}

/** Asynchronous finish function for Fapi_GetPlatformCertificates
 *
 * This function should be called after a previous
 * Fapi_GetPlatformCertificates_Async.
 *
 * @param [in, out] context The FAPI_CONTEXT
 * @param [out] certificates The platform certificates
 * @param [out] certificatesSize The size of the buffer with the certificates.
 *              May be NULL
 *
 * @retval TSS2_RC_SUCCESS: if the function call was a success.
 * @retval TSS2_FAPI_RC_BAD_REFERENCE: if context or certificates is NULL.
 * @retval TSS2_FAPI_RC_BAD_CONTEXT: if context corruption is detected.
 * @retval TSS2_FAPI_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_FAPI_RC_IO_ERROR: if the data cannot be saved.
 * @retval TSS2_FAPI_RC_MEMORY: if the FAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_FAPI_RC_TRY_AGAIN: if the asynchronous operation is not yet
 *         complete. Call this function again later.
 */
TSS2_RC
Fapi_GetPlatformCertificates_Finish(
    FAPI_CONTEXT *context,
    uint8_t **certificates,
    size_t *certificatesSize)
{
    LOG_TRACE("called for context:%p", context);

    NODE_OBJECT_T *cert_list = NULL;
    TSS2_RC r;

    /* Check for NULL parameters */
    check_not_null(context);
    check_not_null(certificates);
    *certificates = NULL;

    switch (context->state) {
        statecase(context->state, GET_PLATFORM_CERTIFICATE);
            r = ifapi_get_certificates(context, MIN_PLATFORM_CERT_HANDLE,
                                       MAX_PLATFORM_CERT_HANDLE,
                                       &cert_list);
            return_try_again(r);
            goto_if_error(r, "Get certificates.", error);

            if (cert_list) {
                /* Concatenate the founc certificates */
                size_t size;
                NODE_OBJECT_T *cert = cert_list;
                size = 0;
                while (cert) {
                    size += cert->size;
                    cert = cert->next;
                }
                if (certificatesSize)
                    *certificatesSize = size;
                *certificates = malloc(size);
                goto_if_null2(*certificates, "Out of memory.",
                        r, TSS2_FAPI_RC_MEMORY, error);

                size = 0;
                while (cert) {
                    memcpy(&cert[size], cert->object, cert->size);
                    size +=  cert->size;
                    cert = cert->next;
                }
            } else {
                if (certificatesSize)
                    *certificatesSize = 0;
                goto_error(r, TSS2_FAPI_RC_NO_CERT,
                        "No platform certificates available.", error);
            }
            break;
        statecasedefault(context->state);
    }
    ifapi_free_object_list(cert_list);
    SAFE_FREE(context->cmd.Provision.capabilityData);
    context->state =  _FAPI_STATE_INIT;
    LOG_TRACE("finished");
    return TSS2_RC_SUCCESS;

 error:
    context->state =  _FAPI_STATE_INIT;
    ifapi_free_object_list(cert_list);
    SAFE_FREE(context->cmd.Provision.capabilityData);
    SAFE_FREE(*certificates);
    return r;
}
