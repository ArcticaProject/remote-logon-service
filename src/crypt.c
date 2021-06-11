/*
 * Copyright © 2012 Canonical Ltd.
 * Copyright © 2015 The Arctica Project
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <glib.h>

#include <gcrypt.h>
#include <math.h>

#include "crypt.h"

static gcry_cipher_hd_t
setup_cipher (const gchar * password)
{
	gcry_error_t     gcryError;
	gcry_cipher_hd_t gcryHandle;

	const size_t keyLength = gcry_cipher_get_algo_keylen(GCRY_CIPHER_AES);
	const size_t blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER_AES);

	// We are assuming keyLength and blkLength are the same, check it
	if (keyLength != blkLength)
		return NULL;

	char * aesSymKey = malloc(blkLength);
	const size_t passwordLength = strlen(password);
	strncpy(aesSymKey, password, blkLength);
	size_t i;
	for (i = passwordLength; i < blkLength; ++i)
		aesSymKey[i] = 0;

	gcryError = gcry_cipher_open(&gcryHandle, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CBC, 0);
	if (gcryError) {
		g_warning("gcry_cipher_open failed: %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
		return NULL;
	}

	gcryError = gcry_cipher_setkey(gcryHandle, aesSymKey, keyLength);
	if (gcryError) {
		g_warning("gcry_cipher_setkey failed: %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
		gcry_cipher_close(gcryHandle);
		return NULL;
	}

	// Use the key as IV too
	gcryError = gcry_cipher_setiv(gcryHandle, aesSymKey, blkLength);
	if (gcryError) {
		g_warning("gcry_cipher_setiv failed: %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
		gcry_cipher_close(gcryHandle);
		return NULL;
	}

	return gcryHandle;
}

/**
 * do_aes_encrypt:
 * @origBuffer: text to encrypt. Needs to be null terminated
 * @password: password to use. Will be cut/padded with 0 if it exceeds/does not reach the needed length
 * @outBufferLength: (out) On success contains the length of the returned buffer
 *
 * Returns the AES encrypted version of the text. It is responsability of the caller to free it
 */
gchar *
do_aes_encrypt(const gchar *origBuffer, const gchar * password, size_t *outBufferLength)
{
	gcry_error_t     gcryError;
	gcry_cipher_hd_t gcryHandle;

	gcryHandle = setup_cipher (password);
	if (gcryHandle == NULL) {
		return NULL;
	}

	const size_t blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER_AES);
	const size_t origBufferLength = strlen(origBuffer);
	const size_t bufferLength = ceil((double)origBufferLength / blkLength) * blkLength;
	gchar *buffer = malloc(bufferLength);
	memcpy(buffer, origBuffer, origBufferLength);
	size_t i;
	for (i = origBufferLength; i < bufferLength; ++i)
		buffer[i] = 0;

	char * encBuffer = malloc(bufferLength);
	size_t lengthDone = 0;
	while (lengthDone < bufferLength) {
		gcryError = gcry_cipher_encrypt(gcryHandle, &encBuffer[lengthDone], blkLength, &buffer[lengthDone], blkLength);
		if (gcryError) {
			g_warning("gcry_cipher_encrypt failed: %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
			gcry_cipher_close(gcryHandle);
			free(encBuffer);
			return NULL;
		}
		lengthDone += blkLength;
	}

	gcry_cipher_close(gcryHandle);

	*outBufferLength = bufferLength;
	return encBuffer;
}

/**
 * do_aes_encrypt:
 * @encBuffer: encrypted data
 * @password: password to use. Will be cut/padded with 0 if it exceeds/does not reach the needed length
 * @encBufferLength: Length of encBuffer
 *
 * Returns the AES decrypted version of the data. It is null terminated. It is responsability of the caller to free it
 */
gchar *
do_aes_decrypt(const gchar *encBuffer, const gchar * password, const size_t encBufferLength)
{
	gcry_error_t     gcryError;
	gcry_cipher_hd_t gcryHandle;

	gcryHandle = setup_cipher (password);
	if (gcryHandle == NULL) {
		return NULL;
	}

	const size_t blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER_AES128);
	const size_t bufferLength = encBufferLength;
	char * outBuffer = malloc(bufferLength);
	size_t lengthDone = 0;
	while (lengthDone < bufferLength) {
		gcryError = gcry_cipher_decrypt(gcryHandle, &outBuffer[lengthDone], 16, &encBuffer[lengthDone], 16);
		if (gcryError)
		{
			g_warning("gcry_cipher_decrypt failed: %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
			return NULL;
		}
		lengthDone += blkLength;
	}

	gcry_cipher_close(gcryHandle);
	char *result = g_strndup(outBuffer, bufferLength);
	free(outBuffer);
	return result;
}
