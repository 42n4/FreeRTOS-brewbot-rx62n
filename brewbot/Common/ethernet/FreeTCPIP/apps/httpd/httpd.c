/**
 * \addtogroup apps
 * @{
 */

/**
 * \defgroup httpd Web server
 * @{
 * The uIP web server is a very simplistic implementation of an HTTP
 * server. It can serve web pages and files from a read-only ROM
 * filesystem, and provides a very small scripting language.

 */

/**
 * \file
 *         Web server
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/*
 * Copyright (c) 2004, Adam Dunkels.
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
 * This file is part of the uIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: httpd.c,v 1.2 2006/06/11 21:46:38 adam Exp $
 */
#include "net/uip.h"
#include "apps/httpd/httpd.h"
#include "apps/httpd/httpd-fs.h"
#include "apps/httpd/httpd-cgi.h"
#include "apps/httpd/http-strings.h"

#include <string.h>

#define STATE_WAITING	0
#define STATE_OUTPUT	1

#define ISO_nl			0x0a
#define ISO_space		0x20
#define ISO_bang		0x21
#define ISO_percent		0x25
#define ISO_period		0x2e
#define ISO_slash		0x2f
#define ISO_colon		0x3a

static char *strnchr(char *haystack, int needle, int len)
{
    int ii = 0;

    for (ii = 0; ii < len; ii++)
    {
	if (needle == haystack[ii])
	    return haystack + ii;
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
static unsigned short generate_part_of_file( void *state )
{
	struct httpd_state	*s = ( struct httpd_state * ) state;
	UINT bytes_read;

	if( s->file.len > uip_mss() )
	{
		s->len = uip_mss();
	}
	else
	{
		s->len = s->file.len;
	}

	// httpd may not have sent all the data last time. Seek to the right spot.
	if (s->file.fat_file.fptr != s->file.fat_file.fsize - s->file.len)
	{
	    f_lseek(&s->file.fat_file, s->file.fat_file.fsize - s->file.len);
	}

	// read the file data into the output buffer
	f_read ( &s->file.fat_file, uip_appdata, s->len, &bytes_read);
	s->len = bytes_read;
	return s->len;
}

/*---------------------------------------------------------------------------*/
static PT_THREAD( send_file ( struct httpd_state *s ) )
{
	PSOCK_BEGIN( &s->sout );

	( void ) PT_YIELD_FLAG;
	
	do
	{
		PSOCK_GENERATOR_SEND( &s->sout, generate_part_of_file, s );
		s->file.len -= s->len;
	} while( s->file.len > 0 );

	PSOCK_END( &s->sout );
}

/*---------------------------------------------------------------------------*/
static PT_THREAD( send_part_of_file ( struct httpd_state *s ) )
{
	PSOCK_BEGIN( &s->sout );
	( void ) PT_YIELD_FLAG;
	
	PSOCK_SEND( &s->sout, uip_appdata, s->len );

	PSOCK_END( &s->sout );
}

/*---------------------------------------------------------------------------*/
static void next_scriptstate( struct httpd_state *s )
{
	char	*p;
	p = strchr( s->scriptptr, ISO_nl ) + 1;
	s->scriptlen -= ( unsigned short ) ( p - s->scriptptr );
	s->scriptptr = p;
}

/*---------------------------------------------------------------------------*/
static PT_THREAD( handle_script ( struct httpd_state *s ) )
{
	char	*ptr;

	PT_BEGIN( &s->scriptpt );
	( void ) PT_YIELD_FLAG;
	while( s->file.len > 0 )
	{
	    char *fdata = uip_appdata;

	    // read part of the file in
	    generate_part_of_file(s);


		/* Check if we should start executing a script. */
		if( *fdata == ISO_percent && *(fdata + 1) == ISO_bang )
		{
			s->scriptptr = fdata + 3;
			s->scriptlen = s->file.len - 3;
			if( *(s->scriptptr - 1) == ISO_colon )
			{
				httpd_fs_open( s->scriptptr + 1, &s->file );
				PT_WAIT_THREAD( &s->scriptpt, send_file(s) );
			}
			else
			{
				PT_WAIT_THREAD( &s->scriptpt, httpd_cgi(s->scriptptr) (s, s->scriptptr) );
			}

			next_scriptstate( s );

			/* The script is over, so we reset the pointers and continue sending the rest of the file. */
			s->file.len = s->scriptlen;
		}
		else
		{
		    /* See if we find the start of script marker in the block of HTML to be sent. */
			if( *fdata == ISO_percent )
			{
			    ptr = strnchr( fdata + 1, ISO_percent, s->len - 1 );
			}
			else
			{
			    ptr = strnchr( fdata, ISO_percent, s->len );
			}

			if( ptr != NULL && ptr != fdata )
			{
				s->len = ( int ) ( ptr - fdata );
			}

			PT_WAIT_THREAD( &s->scriptpt, send_part_of_file(s) );
			s->file.len -= s->len;
		}
	}

	PT_END( &s->scriptpt );
}

/*---------------------------------------------------------------------------*/
static PT_THREAD( send_headers ( struct httpd_state *s, const char *statushdr ) )
{
	char	*ptr;

	PSOCK_BEGIN( &s->sout );
	( void ) PT_YIELD_FLAG;
	PSOCK_SEND_STR( &s->sout, statushdr );

	ptr = strrchr( s->filename, ISO_period );
	if( ptr == NULL )
	{
		PSOCK_SEND_STR( &s->sout, http_content_type_binary );
	}
	else if( strncmp(http_html, ptr, 4) == 0 || strncmp(http_shtml, ptr, 4) == 0 )
	{
		PSOCK_SEND_STR( &s->sout, http_content_type_html );
	}
	else if( strncmp(http_css, ptr, 4) == 0 )
	{
		PSOCK_SEND_STR( &s->sout, http_content_type_css );
	}
	else if( strncmp(http_png, ptr, 4) == 0 )
	{
		PSOCK_SEND_STR( &s->sout, http_content_type_png );
	}
	else if( strncmp(http_gif, ptr, 4) == 0 )
	{
		PSOCK_SEND_STR( &s->sout, http_content_type_gif );
	}
	else if( strncmp(http_jpg, ptr, 4) == 0 )
	{
		PSOCK_SEND_STR( &s->sout, http_content_type_jpg );
	}
	else
	{
		PSOCK_SEND_STR( &s->sout, http_content_type_plain );
	}

	PSOCK_END( &s->sout );
}

/*---------------------------------------------------------------------------*/
static PT_THREAD( handle_output ( struct httpd_state *s ) )
{
	char	*ptr;

	PT_BEGIN( &s->outputpt );
	( void ) PT_YIELD_FLAG;

	// truncate .html to .htm
	ptr = strrchr( s->filename, ISO_period );
	if (ptr != NULL) ptr[4] = 0;


	if( !httpd_fs_open(s->filename, &s->file) )
	{
		httpd_fs_open( http_404_html, &s->file );
		strcpy( s->filename, http_404_html );
		PT_WAIT_THREAD( &s->outputpt, send_headers(s, http_header_404) );
		PT_WAIT_THREAD( &s->outputpt, send_file(s) );
	}
	else
	{
		PT_WAIT_THREAD( &s->outputpt, send_headers(s, http_header_200) );
		ptr = strchr( s->filename, ISO_period );
		if( ptr != NULL && strncmp(ptr, http_shtml, 6) == 0 )
		{
			PT_INIT( &s->scriptpt );
			PT_WAIT_THREAD( &s->outputpt, handle_script(s) );
		}
		else
		{
			PT_WAIT_THREAD( &s->outputpt, send_file(s) );
		}
	}

	PSOCK_CLOSE( &s->sout );
	PT_END( &s->outputpt );
}

/*---------------------------------------------------------------------------*/
static PT_THREAD( handle_input ( struct httpd_state *s ) )
{
	PSOCK_BEGIN( &s->sin );
	( void ) PT_YIELD_FLAG;
	PSOCK_READTO( &s->sin, ISO_space );

	if( strncmp(s->inputbuf, http_get, 4) != 0 )
	{
		PSOCK_CLOSE_EXIT( &s->sin );
	}

	PSOCK_READTO( &s->sin, ISO_space );

	if( s->inputbuf[0] != ISO_slash )
	{
		PSOCK_CLOSE_EXIT( &s->sin );
	}

	if( s->inputbuf[1] == ISO_space )
	{
		strncpy( s->filename, http_index_html, sizeof(s->filename) );
	}
	else
	{
		s->inputbuf[PSOCK_DATALEN( &s->sin ) - 1] = 0;
		
		/* Process any form input being sent to the server. */
		#if UIP_CONF_PROCESS_HTTPD_FORMS == 1
		{
			extern void vApplicationProcessFormInput( char *pcInputString );
			vApplicationProcessFormInput( s->inputbuf );
		}
		#endif
		
		strncpy( s->filename, &s->inputbuf[0], sizeof(s->filename) );
	}

	/*  httpd_log_file(uip_conn->ripaddr, s->filename);*/
	s->state = STATE_OUTPUT;

	while( 1 )
	{
		PSOCK_READTO( &s->sin, ISO_nl );

		if( strncmp(s->inputbuf, http_referer, 8) == 0 )
		{
			s->inputbuf[PSOCK_DATALEN( &s->sin ) - 2] = 0;

			/*      httpd_log(&s->inputbuf[9]);*/
		}
	}

	PSOCK_END( &s->sin );
}

/*---------------------------------------------------------------------------*/
static void handle_connection( struct httpd_state *s )
{
	handle_input( s );
	if( s->state == STATE_OUTPUT )
	{
		handle_output( s );
	}
}

/*---------------------------------------------------------------------------*/
void httpd_appcall( void )
{
	struct httpd_state	*s = ( struct httpd_state * ) &( uip_conn->appstate );

	if( uip_closed() || uip_aborted() || uip_timedout() )
	{
	}
	else if( uip_connected() )
	{
		PSOCK_INIT( &s->sin, s->inputbuf, sizeof(s->inputbuf) - 1 );
		PSOCK_INIT( &s->sout, s->inputbuf, sizeof(s->inputbuf) - 1 );
		PT_INIT( &s->outputpt );
		s->state = STATE_WAITING;

		/*    timer_set(&s->timer, CLOCK_SECOND * 100);*/
		s->timer = 0;
		handle_connection( s );
	}
	else if( s != NULL )
	{
		if( uip_poll() )
		{
			++s->timer;
			if( s->timer >= 20 )
			{
				uip_abort();
			}
		}
		else
		{
			s->timer = 0;
		}

		handle_connection( s );
	}
	else
	{
		uip_abort();
	}
}

/*---------------------------------------------------------------------------*/

/**
 * \brief      Initialize the web server
 *
 *             This function initializes the web server and should be
 *             called at system boot-up.
 */
void httpd_init( void )
{
	uip_listen( HTONS(80) );
}

/*---------------------------------------------------------------------------*/

/** @} */
