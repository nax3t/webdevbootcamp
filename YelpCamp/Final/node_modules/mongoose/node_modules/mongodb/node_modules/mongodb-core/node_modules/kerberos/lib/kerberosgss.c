/**
 * Copyright (c) 2006-2010 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **/
/*
 * S4U2Proxy implementation
 * Copyright (C) 2015 David Mansfield
 * Code inspired by mod_auth_kerb.
 */

#include "kerberosgss.h"

#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <krb5.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

void die1(const char *message) {
  if(errno) {
    perror(message);
  } else {
    printf("ERROR: %s\n", message);
  }

  exit(1);
}

static gss_client_response *gss_error(const char *func, const char *op, OM_uint32 err_maj, OM_uint32 err_min);
static gss_client_response *other_error(const char *fmt, ...);
static gss_client_response *krb5_ctx_error(krb5_context context, krb5_error_code problem);

static gss_client_response *store_gss_creds(gss_server_state *state);
static gss_client_response *create_krb5_ccache(gss_server_state *state, krb5_context context, krb5_principal princ, krb5_ccache *ccache);

/*
char* server_principal_details(const char* service, const char* hostname)
{
    char match[1024];
    int match_len = 0;
    char* result = NULL;
    
    int code;
    krb5_context kcontext;
    krb5_keytab kt = NULL;
    krb5_kt_cursor cursor = NULL;
    krb5_keytab_entry entry;
    char* pname = NULL;
    
    // Generate the principal prefix we want to match
    snprintf(match, 1024, "%s/%s@", service, hostname);
    match_len = strlen(match);
    
    code = krb5_init_context(&kcontext);
    if (code)
    {
        PyErr_SetObject(KrbException_class, Py_BuildValue("((s:i))",
                                                          "Cannot initialize Kerberos5 context", code));
        return NULL;
    }
    
    if ((code = krb5_kt_default(kcontext, &kt)))
    {
        PyErr_SetObject(KrbException_class, Py_BuildValue("((s:i))",
                                                          "Cannot get default keytab", code));
        goto end;
    }
    
    if ((code = krb5_kt_start_seq_get(kcontext, kt, &cursor)))
    {
        PyErr_SetObject(KrbException_class, Py_BuildValue("((s:i))",
                                                          "Cannot get sequence cursor from keytab", code));
        goto end;
    }
    
    while ((code = krb5_kt_next_entry(kcontext, kt, &entry, &cursor)) == 0)
    {
        if ((code = krb5_unparse_name(kcontext, entry.principal, &pname)))
        {
            PyErr_SetObject(KrbException_class, Py_BuildValue("((s:i))",
                                                              "Cannot parse principal name from keytab", code));
            goto end;
        }
        
        if (strncmp(pname, match, match_len) == 0)
        {
            result = malloc(strlen(pname) + 1);
            strcpy(result, pname);
            krb5_free_unparsed_name(kcontext, pname);
            krb5_free_keytab_entry_contents(kcontext, &entry);
            break;
        }
        
        krb5_free_unparsed_name(kcontext, pname);
        krb5_free_keytab_entry_contents(kcontext, &entry);
    }
    
    if (result == NULL)
    {
        PyErr_SetObject(KrbException_class, Py_BuildValue("((s:i))",
                                                          "Principal not found in keytab", -1));
    }
    
end:
    if (cursor)
        krb5_kt_end_seq_get(kcontext, kt, &cursor);
    if (kt)
        krb5_kt_close(kcontext, kt);
    krb5_free_context(kcontext);
    
    return result;
}
*/
gss_client_response *authenticate_gss_client_init(const char* service, long int gss_flags, gss_client_state* state) {
  OM_uint32 maj_stat;
  OM_uint32 min_stat;
  gss_buffer_desc name_token = GSS_C_EMPTY_BUFFER;
  gss_client_response *response = NULL;
  int ret = AUTH_GSS_COMPLETE;

  state->server_name = GSS_C_NO_NAME;
  state->context = GSS_C_NO_CONTEXT;
  state->gss_flags = gss_flags;
  state->username = NULL;
  state->response = NULL;
  
  // Import server name first
  name_token.length = strlen(service);
  name_token.value = (char *)service;
  
  maj_stat = gss_import_name(&min_stat, &name_token, gss_krb5_nt_service_name, &state->server_name);
  
  if (GSS_ERROR(maj_stat)) {
    response = gss_error(__func__, "gss_import_name", maj_stat, min_stat);
    response->return_code = AUTH_GSS_ERROR;
    goto end;
  }
  
end:
  if(response == NULL) {
    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    response->return_code = ret;    
  }

  return response;
}

gss_client_response *authenticate_gss_client_clean(gss_client_state *state) {
  OM_uint32 min_stat;
  int ret = AUTH_GSS_COMPLETE;
  gss_client_response *response = NULL;
  
  if(state->context != GSS_C_NO_CONTEXT)
    gss_delete_sec_context(&min_stat, &state->context, GSS_C_NO_BUFFER);
  
  if(state->server_name != GSS_C_NO_NAME)
    gss_release_name(&min_stat, &state->server_name);
  
  if(state->username != NULL) {
    free(state->username);
    state->username = NULL;
  }

  if (state->response != NULL) {
    free(state->response);
    state->response = NULL;
  }
  
  if(response == NULL) {
    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    response->return_code = ret;    
  }

  return response;
}

gss_client_response *authenticate_gss_client_step(gss_client_state* state, const char* challenge) {
  OM_uint32 maj_stat;
  OM_uint32 min_stat;
  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  int ret = AUTH_GSS_CONTINUE;
  gss_client_response *response = NULL;
  
  // Always clear out the old response
  if (state->response != NULL) {
    free(state->response);
    state->response = NULL;
  }
  
  // If there is a challenge (data from the server) we need to give it to GSS
  if (challenge && *challenge) {
    int len;
    input_token.value = base64_decode(challenge, &len);
    input_token.length = len;
  }
  
  // Do GSSAPI step
  maj_stat = gss_init_sec_context(&min_stat,
                                  GSS_C_NO_CREDENTIAL,
                                  &state->context,
                                  state->server_name,
                                  GSS_C_NO_OID,
                                  (OM_uint32)state->gss_flags,
                                  0,
                                  GSS_C_NO_CHANNEL_BINDINGS,
                                  &input_token,
                                  NULL,
                                  &output_token,
                                  NULL,
                                  NULL);

  if ((maj_stat != GSS_S_COMPLETE) && (maj_stat != GSS_S_CONTINUE_NEEDED)) {
    response = gss_error(__func__, "gss_init_sec_context", maj_stat, min_stat);
    response->return_code = AUTH_GSS_ERROR;
    goto end;
  }
  
  ret = (maj_stat == GSS_S_COMPLETE) ? AUTH_GSS_COMPLETE : AUTH_GSS_CONTINUE;
  // Grab the client response to send back to the server
  if(output_token.length) {
    state->response = base64_encode((const unsigned char *)output_token.value, output_token.length);
    maj_stat = gss_release_buffer(&min_stat, &output_token);
  }
  
  // Try to get the user name if we have completed all GSS operations
  if (ret == AUTH_GSS_COMPLETE) {
    gss_name_t gssuser = GSS_C_NO_NAME;
    maj_stat = gss_inquire_context(&min_stat, state->context, &gssuser, NULL, NULL, NULL,  NULL, NULL, NULL);
    
    if(GSS_ERROR(maj_stat)) {
      response = gss_error(__func__, "gss_inquire_context", maj_stat, min_stat);
      response->return_code = AUTH_GSS_ERROR;
      goto end;
    }
    
    gss_buffer_desc name_token;
    name_token.length = 0;
    maj_stat = gss_display_name(&min_stat, gssuser, &name_token, NULL);
    
    if(GSS_ERROR(maj_stat)) {
      if(name_token.value)
        gss_release_buffer(&min_stat, &name_token);
      gss_release_name(&min_stat, &gssuser);
      
      response = gss_error(__func__, "gss_display_name", maj_stat, min_stat);
      response->return_code = AUTH_GSS_ERROR;
      goto end;
    } else {
      state->username = (char *)malloc(name_token.length + 1);
      if(state->username == NULL) die1("Memory allocation failed");
      strncpy(state->username, (char*) name_token.value, name_token.length);
      state->username[name_token.length] = 0;
      gss_release_buffer(&min_stat, &name_token);
      gss_release_name(&min_stat, &gssuser);
    }
  }

end:
  if(output_token.value)
    gss_release_buffer(&min_stat, &output_token);
  if(input_token.value)
    free(input_token.value);

  if(response == NULL) {
    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    response->return_code = ret;
  }

  // Return the response
  return response;
}

gss_client_response *authenticate_gss_client_unwrap(gss_client_state *state, const char *challenge) {
  OM_uint32 maj_stat;
  OM_uint32 min_stat;
  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  gss_client_response *response = NULL;
  int ret = AUTH_GSS_CONTINUE;
    
  // Always clear out the old response
  if(state->response != NULL) {
    free(state->response);
    state->response = NULL;
  }
    
  // If there is a challenge (data from the server) we need to give it to GSS
  if(challenge && *challenge) {
    int len;
    input_token.value = base64_decode(challenge, &len);
    input_token.length = len;
  }
    
  // Do GSSAPI step
  maj_stat = gss_unwrap(&min_stat,
                          state->context,
                          &input_token,
                          &output_token,
                          NULL,
                          NULL);
    
  if(maj_stat != GSS_S_COMPLETE) {
    response = gss_error(__func__, "gss_unwrap", maj_stat, min_stat);
    response->return_code = AUTH_GSS_ERROR;
    goto end;
  } else {
    ret = AUTH_GSS_COMPLETE;    
  }
    
  // Grab the client response
  if(output_token.length) {
    state->response = base64_encode((const unsigned char *)output_token.value, output_token.length);
    gss_release_buffer(&min_stat, &output_token);
  }
end:
  if(output_token.value)
    gss_release_buffer(&min_stat, &output_token);
  if(input_token.value)
    free(input_token.value);

  if(response == NULL) {
    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    response->return_code = ret;
  }

  // Return the response
  return response;
}

gss_client_response *authenticate_gss_client_wrap(gss_client_state* state, const char* challenge, const char* user) {
  OM_uint32 maj_stat;
  OM_uint32 min_stat;
  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  int ret = AUTH_GSS_CONTINUE;
  gss_client_response *response = NULL;
  char buf[4096], server_conf_flags;
  unsigned long buf_size;
    
  // Always clear out the old response
  if(state->response != NULL) {
    free(state->response);
    state->response = NULL;
  }
    
  if(challenge && *challenge) {
    int len;
    input_token.value = base64_decode(challenge, &len);
    input_token.length = len;
  }
    
  if(user) {
    // get bufsize
    server_conf_flags = ((char*) input_token.value)[0];
    ((char*) input_token.value)[0] = 0;
    buf_size = ntohl(*((long *) input_token.value));
    free(input_token.value);
#ifdef PRINTFS
    printf("User: %s, %c%c%c\n", user,
               server_conf_flags & GSS_AUTH_P_NONE      ? 'N' : '-',
               server_conf_flags & GSS_AUTH_P_INTEGRITY ? 'I' : '-',
               server_conf_flags & GSS_AUTH_P_PRIVACY   ? 'P' : '-');
    printf("Maximum GSS token size is %ld\n", buf_size);
#endif
        
    // agree to terms (hack!)
    buf_size = htonl(buf_size); // not relevant without integrity/privacy
    memcpy(buf, &buf_size, 4);
    buf[0] = GSS_AUTH_P_NONE;
    // server decides if principal can log in as user
    strncpy(buf + 4, user, sizeof(buf) - 4);
    input_token.value = buf;
    input_token.length = 4 + strlen(user);
  }
    
  // Do GSSAPI wrap
  maj_stat = gss_wrap(&min_stat,
            state->context,
            0,
            GSS_C_QOP_DEFAULT,
            &input_token,
            NULL,
            &output_token);
    
  if (maj_stat != GSS_S_COMPLETE) {
    response = gss_error(__func__, "gss_wrap", maj_stat, min_stat);
    response->return_code = AUTH_GSS_ERROR;
    goto end;
  } else
    ret = AUTH_GSS_COMPLETE;
  // Grab the client response to send back to the server
  if (output_token.length) {
    state->response = base64_encode((const unsigned char *)output_token.value, output_token.length);;
    gss_release_buffer(&min_stat, &output_token);
  }
end:
  if (output_token.value)
    gss_release_buffer(&min_stat, &output_token);

  if(response == NULL) {
    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    response->return_code = ret;
  }

  // Return the response
  return response;
}

gss_client_response *authenticate_gss_server_init(const char *service, bool constrained_delegation, const char *username, gss_server_state *state)
{
    OM_uint32 maj_stat;
    OM_uint32 min_stat;
    gss_buffer_desc name_token = GSS_C_EMPTY_BUFFER;
    int ret = AUTH_GSS_COMPLETE;
    gss_client_response *response = NULL;
    gss_cred_usage_t usage = GSS_C_ACCEPT;

    state->context = GSS_C_NO_CONTEXT;
    state->server_name = GSS_C_NO_NAME;
    state->client_name = GSS_C_NO_NAME;
    state->server_creds = GSS_C_NO_CREDENTIAL;
    state->client_creds = GSS_C_NO_CREDENTIAL;
    state->username = NULL;
    state->targetname = NULL;
    state->response = NULL;
    state->constrained_delegation = constrained_delegation;
    state->delegated_credentials_cache = NULL;
    
    // Server name may be empty which means we aren't going to create our own creds
    size_t service_len = strlen(service);
    if (service_len != 0)
    {
        // Import server name first
        name_token.length = strlen(service);
        name_token.value = (char *)service;
        
        maj_stat = gss_import_name(&min_stat, &name_token, GSS_C_NT_HOSTBASED_SERVICE, &state->server_name);
        
        if (GSS_ERROR(maj_stat))
        {
            response = gss_error(__func__, "gss_import_name", maj_stat, min_stat);
            response->return_code = AUTH_GSS_ERROR;
            goto end;
        }
        
        if (state->constrained_delegation)
        {
            usage = GSS_C_BOTH;
        }

        // Get credentials
        maj_stat = gss_acquire_cred(&min_stat, state->server_name, GSS_C_INDEFINITE,
                                    GSS_C_NO_OID_SET, usage, &state->server_creds, NULL, NULL);

        if (GSS_ERROR(maj_stat))
        {
            response = gss_error(__func__, "gss_acquire_cred", maj_stat, min_stat);
            response->return_code = AUTH_GSS_ERROR;
            goto end;
        }
    }
    
    // If a username was passed, perform the S4U2Self protocol transition to acquire
    // a credentials from that user as if we had done gss_accept_sec_context.
    // In this scenario, the passed username is assumed to be already authenticated
    // by some external mechanism, and we are here to "bootstrap" some gss credentials.
    // In authenticate_gss_server_step we will bypass the actual authentication step.
    if (username != NULL)
    {
	gss_name_t gss_username;

	name_token.length = strlen(username);
	name_token.value = (char *)username;

	maj_stat = gss_import_name(&min_stat, &name_token, GSS_C_NT_USER_NAME, &gss_username);
	if (GSS_ERROR(maj_stat))
	{
	    response = gss_error(__func__, "gss_import_name", maj_stat, min_stat);
	    response->return_code = AUTH_GSS_ERROR;
	    goto end;
	}

	maj_stat = gss_acquire_cred_impersonate_name(&min_stat,
		state->server_creds,
		gss_username,
		GSS_C_INDEFINITE,
		GSS_C_NO_OID_SET,
		GSS_C_INITIATE,
		&state->client_creds,
		NULL,
		NULL);

	if (GSS_ERROR(maj_stat))
	{
	    response = gss_error(__func__, "gss_acquire_cred_impersonate_name", maj_stat, min_stat);
	    response->return_code = AUTH_GSS_ERROR;
	}

	gss_release_name(&min_stat, &gss_username);

	if (response != NULL)
	{
	    goto end;
	}

	// because the username MAY be a "local" username,
	// we want get the canonical name from the acquired creds.
	maj_stat = gss_inquire_cred(&min_stat, state->client_creds, &state->client_name, NULL, NULL, NULL);
	if (GSS_ERROR(maj_stat))
	{
	    response = gss_error(__func__, "gss_inquire_cred", maj_stat, min_stat);
	    response->return_code = AUTH_GSS_ERROR;
	    goto end;
	}
    }

end:
    if(response == NULL) {
      response = calloc(1, sizeof(gss_client_response));
      if(response == NULL) die1("Memory allocation failed");
      response->return_code = ret;
    }

    // Return the response
    return response;
}

gss_client_response *authenticate_gss_server_clean(gss_server_state *state)
{
    OM_uint32 min_stat;
    int ret = AUTH_GSS_COMPLETE;
    gss_client_response *response = NULL;
    
    if (state->context != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&min_stat, &state->context, GSS_C_NO_BUFFER);
    if (state->server_name != GSS_C_NO_NAME)
        gss_release_name(&min_stat, &state->server_name);
    if (state->client_name != GSS_C_NO_NAME)
        gss_release_name(&min_stat, &state->client_name);
    if (state->server_creds != GSS_C_NO_CREDENTIAL)
        gss_release_cred(&min_stat, &state->server_creds);
    if (state->client_creds != GSS_C_NO_CREDENTIAL)
        gss_release_cred(&min_stat, &state->client_creds);
    if (state->username != NULL)
    {
        free(state->username);
        state->username = NULL;
    }
    if (state->targetname != NULL)
    {
        free(state->targetname);
        state->targetname = NULL;
    }
    if (state->response != NULL)
    {
        free(state->response);
        state->response = NULL;
    }
    if (state->delegated_credentials_cache)
    {
	// TODO: what about actually destroying the cache? It can't be done now as
	// the whole point is having it around for the lifetime of the "session"
	free(state->delegated_credentials_cache);
    }
    
    if(response == NULL) {
      response = calloc(1, sizeof(gss_client_response));
      if(response == NULL) die1("Memory allocation failed");
      response->return_code = ret;
    }

    // Return the response
    return response;
}

gss_client_response *authenticate_gss_server_step(gss_server_state *state, const char *auth_data)
{
    OM_uint32 maj_stat;
    OM_uint32 min_stat;
    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    int ret = AUTH_GSS_CONTINUE;
    gss_client_response *response = NULL;
    
    // Always clear out the old response
    if (state->response != NULL)
    {
        free(state->response);
        state->response = NULL;
    }
    
    // we don't need to check the authentication token if S4U2Self protocol
    // transition was done, because we already have the client credentials.
    if (state->client_creds == GSS_C_NO_CREDENTIAL)
    {
	if (auth_data && *auth_data)
	{
	    int len;
	    input_token.value = base64_decode(auth_data, &len);
	    input_token.length = len;
	}
	else
	{
	    response = calloc(1, sizeof(gss_client_response));
	    if(response == NULL) die1("Memory allocation failed");
	    response->message = strdup("No auth_data value in request from client");
	    response->return_code = AUTH_GSS_ERROR;
	    goto end;
	}

	maj_stat = gss_accept_sec_context(&min_stat,
					  &state->context,
					  state->server_creds,
					  &input_token,
					  GSS_C_NO_CHANNEL_BINDINGS,
					  &state->client_name,
					  NULL,
					  &output_token,
					  NULL,
					  NULL,
					  &state->client_creds);

	if (GSS_ERROR(maj_stat))
	{
	    response = gss_error(__func__, "gss_accept_sec_context", maj_stat, min_stat);
	    response->return_code = AUTH_GSS_ERROR;
	    goto end;
	}

	// Grab the server response to send back to the client
	if (output_token.length)
	{
	    state->response = base64_encode((const unsigned char *)output_token.value, output_token.length);
	    maj_stat = gss_release_buffer(&min_stat, &output_token);
	}
    }

    // Get the user name
    maj_stat = gss_display_name(&min_stat, state->client_name, &output_token, NULL);
    if (GSS_ERROR(maj_stat))
    {
	response = gss_error(__func__, "gss_display_name", maj_stat, min_stat);
	response->return_code = AUTH_GSS_ERROR;
	goto end;
    }
    state->username = (char *)malloc(output_token.length + 1);
    strncpy(state->username, (char*) output_token.value, output_token.length);
    state->username[output_token.length] = 0;

    // Get the target name if no server creds were supplied
    if (state->server_creds == GSS_C_NO_CREDENTIAL)
    {
	gss_name_t target_name = GSS_C_NO_NAME;
	maj_stat = gss_inquire_context(&min_stat, state->context, NULL, &target_name, NULL, NULL, NULL, NULL, NULL);
	if (GSS_ERROR(maj_stat))
	{
	    response = gss_error(__func__, "gss_inquire_context", maj_stat, min_stat);
	    response->return_code = AUTH_GSS_ERROR;
	    goto end;
	}
	maj_stat = gss_display_name(&min_stat, target_name, &output_token, NULL);
	if (GSS_ERROR(maj_stat))
	{
	    response = gss_error(__func__, "gss_display_name", maj_stat, min_stat);
	    response->return_code = AUTH_GSS_ERROR;
	    goto end;
	}
	state->targetname = (char *)malloc(output_token.length + 1);
	strncpy(state->targetname, (char*) output_token.value, output_token.length);
	state->targetname[output_token.length] = 0;
    }

    if (state->constrained_delegation && state->client_creds != GSS_C_NO_CREDENTIAL)
    {
	if ((response = store_gss_creds(state)) != NULL)
	{
	    goto end;
	}
    }

    ret = AUTH_GSS_COMPLETE;
    
end:
    if (output_token.length)
        gss_release_buffer(&min_stat, &output_token);
    if (input_token.value)
        free(input_token.value);

    if(response == NULL) {
      response = calloc(1, sizeof(gss_client_response));
      if(response == NULL) die1("Memory allocation failed");
      response->return_code = ret;
    }

    // Return the response
    return response;
}

static gss_client_response *store_gss_creds(gss_server_state *state)
{
    OM_uint32 maj_stat, min_stat;
    krb5_principal princ = NULL;
    krb5_ccache ccache = NULL;
    krb5_error_code problem;
    krb5_context context;
    gss_client_response *response = NULL;

    problem = krb5_init_context(&context);
    if (problem) {
	response = other_error("No auth_data value in request from client");
        return response;
    }

    problem = krb5_parse_name(context, state->username, &princ);
    if (problem) {
	response = krb5_ctx_error(context, problem);
	goto end;
    }

    if ((response = create_krb5_ccache(state, context, princ, &ccache)))
    {
	goto end;
    }

    maj_stat = gss_krb5_copy_ccache(&min_stat, state->client_creds, ccache);
    if (GSS_ERROR(maj_stat)) {
        response = gss_error(__func__, "gss_krb5_copy_ccache", maj_stat, min_stat);
        response->return_code = AUTH_GSS_ERROR;
        goto end;
    }

    krb5_cc_close(context, ccache);
    ccache = NULL;

    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    // TODO: something other than AUTH_GSS_COMPLETE?
    response->return_code = AUTH_GSS_COMPLETE;

 end:
    if (princ)
	krb5_free_principal(context, princ);
    if (ccache)
	krb5_cc_destroy(context, ccache);
    krb5_free_context(context);

    return response;
}

static gss_client_response *create_krb5_ccache(gss_server_state *state, krb5_context kcontext, krb5_principal princ, krb5_ccache *ccache)
{
    char *ccname = NULL;
    int fd;
    krb5_error_code problem;
    krb5_ccache tmp_ccache = NULL;
    gss_client_response *error = NULL;

    // TODO: mod_auth_kerb used a temp file under /run/httpd/krbcache. what can we do?
    ccname = strdup("FILE:/tmp/krb5cc_nodekerberos_XXXXXX");
    if (!ccname) die1("Memory allocation failed");

    fd = mkstemp(ccname + strlen("FILE:"));
    if (fd < 0) {
	error = other_error("mkstemp() failed: %s", strerror(errno));
	goto end;
    }

    close(fd);

    problem = krb5_cc_resolve(kcontext, ccname, &tmp_ccache);
    if (problem) {
       error = krb5_ctx_error(kcontext, problem);
       goto end;
    }

    problem = krb5_cc_initialize(kcontext, tmp_ccache, princ);
    if (problem) {
	error = krb5_ctx_error(kcontext, problem);
	goto end;
    }

    state->delegated_credentials_cache = strdup(ccname);

    // TODO: how/when to cleanup the creds cache file?
    // TODO: how to expose the credentials expiration time?

    *ccache = tmp_ccache;
    tmp_ccache = NULL;

 end:
    if (tmp_ccache)
	krb5_cc_destroy(kcontext, tmp_ccache);

    if (ccname && error)
	unlink(ccname);

    if (ccname)
	free(ccname);

    return error;
}


gss_client_response *gss_error(const char *func, const char *op, OM_uint32 err_maj, OM_uint32 err_min) {
  OM_uint32 maj_stat, min_stat;
  OM_uint32 msg_ctx = 0;
  gss_buffer_desc status_string;

  gss_client_response *response = calloc(1, sizeof(gss_client_response));
  if(response == NULL) die1("Memory allocation failed");
  
  char *message = NULL;
  message = calloc(1024, 1);
  if(message == NULL) die1("Memory allocation failed");

  response->message = message;

  int nleft = 1024;
  int n;

  n = snprintf(message, nleft, "%s(%s)", func, op);
  message += n;
  nleft -= n;

  do {
    maj_stat = gss_display_status (&min_stat,
                                   err_maj,
                                   GSS_C_GSS_CODE,
                                   GSS_C_NO_OID,
                                   &msg_ctx,
                                   &status_string);
    if(GSS_ERROR(maj_stat))
      break;
    
    n = snprintf(message, nleft, ": %.*s",
	    (int)status_string.length, (char*)status_string.value);
    message += n;
    nleft -= n;

    gss_release_buffer(&min_stat, &status_string);
    
    maj_stat = gss_display_status (&min_stat,
                                   err_min,
                                   GSS_C_MECH_CODE,
                                   GSS_C_NULL_OID,
                                   &msg_ctx,
                                   &status_string);
    if(!GSS_ERROR(maj_stat)) {
	n = snprintf(message, nleft, ": %.*s",
		(int)status_string.length, (char*)status_string.value);
	message += n;
	nleft -= n;

      gss_release_buffer(&min_stat, &status_string);
    }
  } while (!GSS_ERROR(maj_stat) && msg_ctx != 0);

  return response;
}

static gss_client_response *krb5_ctx_error(krb5_context context, krb5_error_code problem)
{
    gss_client_response *response = NULL;
    const char *error_text = krb5_get_error_message(context, problem);
    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    response->message = strdup(error_text);
    // TODO: something other than AUTH_GSS_ERROR? AUTH_KRB5_ERROR ?
    response->return_code = AUTH_GSS_ERROR;
    krb5_free_error_message(context, error_text);
    return response;
}

static gss_client_response *other_error(const char *fmt, ...)
{
    size_t needed;
    char *msg;
    gss_client_response *response = NULL;
    va_list ap, aps;

    va_start(ap, fmt);

    va_copy(aps, ap);
    needed = snprintf(NULL, 0, fmt, aps);
    va_end(aps);

    msg = malloc(needed);
    if (!msg) die1("Memory allocation failed");

    vsnprintf(msg, needed, fmt, ap);
    va_end(ap);

    response = calloc(1, sizeof(gss_client_response));
    if(response == NULL) die1("Memory allocation failed");
    response->message = msg;

    // TODO: something other than AUTH_GSS_ERROR?
    response->return_code = AUTH_GSS_ERROR;

    return response;
}


#pragma clang diagnostic pop

