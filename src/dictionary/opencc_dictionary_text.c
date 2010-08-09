/*
* Open Chinese Convert
*
* Copyright 2010 BYVoid <byvoid1@gmail.com>
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

#include "opencc_dictionary_text.h"
#include "../opencc_encoding.h"

#define INITIAL_DICTIONARY_SIZE 1024
#define ENTRY_BUFF_SIZE 128
#define ENTRY_WBUFF_SIZE ENTRY_BUFF_SIZE / sizeof(size_t)

typedef struct
{
	size_t entry_count;
	size_t max_length;
	opencc_entry * lexicon;
	ucs4_t * word_buff;
} text_dictionary;

int qsort_entry_cmp(const void *a, const void *b)
{
	return ucs4cmp(((opencc_entry *)a)->key, ((opencc_entry *)b)->key);
}

dict_ptr dict_text_open(const char * filename)
{
	text_dictionary * td;
	td = (text_dictionary *) malloc(sizeof(text_dictionary));
	td->entry_count = INITIAL_DICTIONARY_SIZE;
	td->max_length = 0;
	td->lexicon = (opencc_entry *) malloc(sizeof(opencc_entry) * td->entry_count);
	td->word_buff = NULL;

	static char buff[ENTRY_BUFF_SIZE];
	static char key_buff[ENTRY_BUFF_SIZE];
	static char value_buff[ENTRY_BUFF_SIZE];
	ucs4_t * wbuff;

	FILE * fp = fopen(filename,"rb");
	if (fp == NULL)
	{
		dict_text_close((dict_ptr) td);
		return (dict_ptr) -1;
	}

	size_t i = 0;
	while (fgets(buff, ENTRY_BUFF_SIZE, fp))
	{
		if (i >= td->entry_count)
		{
			td->entry_count += td->entry_count;
			td->lexicon = (opencc_entry *) realloc(td->lexicon, sizeof(opencc_entry) * td->entry_count);
		}

		sscanf(buff, "%s %s", key_buff, value_buff);

		wbuff = utf8_to_ucs4(key_buff,(size_t) -1);

		if (wbuff == (ucs4_t *) -1)
		{
			td->entry_count = i + 1;
			dict_text_close((dict_ptr) td);
			return (dict_ptr) -1;
		}

		size_t length = ucs4len(wbuff);
		if (length > td->max_length)
			td->max_length = length;

		td->lexicon[i].key = (ucs4_t *) malloc((length + 1) * sizeof(ucs4_t));
		ucs4cpy(td->lexicon[i].key, wbuff);
		free(wbuff);

		wbuff = utf8_to_ucs4(value_buff,(size_t) -1);

		if (wbuff == (ucs4_t *) -1)
		{
			td->entry_count = i + 1;
			dict_text_close((dict_ptr) td);
			return (dict_ptr) -1;
		}

		td->lexicon[i].value = (ucs4_t *) malloc((ucs4len(wbuff) + 1) * sizeof(ucs4_t));
		ucs4cpy(td->lexicon[i].value, wbuff);
		free(wbuff);

		i ++;
	}

	fclose(fp);

	td->entry_count = i;
	td->lexicon = (opencc_entry *) realloc(td->lexicon, sizeof(opencc_entry) * td->entry_count);
	td->word_buff = (ucs4_t *) malloc(sizeof(ucs4_t) * (td->max_length + 1));

	qsort(td->lexicon, td->entry_count, sizeof(td->lexicon[0]), qsort_entry_cmp);

	return (dict_ptr) td;
}

void dict_text_close(dict_ptr dp)
{
	text_dictionary * td = (text_dictionary *) dp;

	size_t i;
	for (i = 0; i < td->entry_count; i ++)
	{
		free(td->lexicon[i].key);
		free(td->lexicon[i].value);
	}

	free(td->lexicon);
	free(td->word_buff);
	free(td);
}

const ucs4_t * dict_text_match_longest(dict_ptr dp, const ucs4_t * word,
		size_t maxlen, size_t * match_length)
{
	text_dictionary * td = (text_dictionary *) dp;

	if (td->entry_count == 0)
		return NULL;

	if (maxlen == 0)
		maxlen = ucs4len(word);
	size_t len = td->max_length;
	if (maxlen < len)
		len = maxlen;

	ucs4ncpy(td->word_buff, word, len);
	td->word_buff[len] = L'\0';

	opencc_entry buff;
	buff.key = td->word_buff;

	for (; len > 0; len --)
	{
		td->word_buff[len] = L'\0';
		opencc_entry * brs = (opencc_entry *) bsearch(&buff, td->lexicon, td->entry_count,
				sizeof(td->lexicon[0]), qsort_entry_cmp);

		if (brs != NULL)
		{
			if (match_length != NULL)
				*match_length = len;
			return brs->value;
		}
	}

	if (match_length != NULL)
		*match_length = 0;
	return NULL;
}

size_t dict_text_get_all_match_lengths(dict_ptr dp, const ucs4_t * word,
		size_t * match_length)
{
	text_dictionary * td = (text_dictionary *) dp;

	size_t rscnt = 0;

	if (td->entry_count == 0)
		return rscnt;

	size_t length = ucs4len(word);
	size_t len = td->max_length;
	if (length < len)
		len = length;

	ucs4ncpy(td->word_buff, word, len);
	td->word_buff[len] = L'\0';

	opencc_entry buff;
	buff.key = td->word_buff;

	for (; len > 0; len --)
	{
		td->word_buff[len] = L'\0';
		opencc_entry * brs = (opencc_entry *) bsearch(&buff, td->lexicon, td->entry_count,
				sizeof(td->lexicon[0]), qsort_entry_cmp);

		if (brs != NULL)
			match_length[rscnt ++] = len;
	}

	return rscnt;
}

size_t dict_text_get_lexicon(dict_ptr dp, opencc_entry * lexicon)
{
	text_dictionary * td = (text_dictionary *) dp;

	size_t i;
	for (i = 0; i < td->entry_count; i ++)
	{
		lexicon[i].key = td->lexicon[i].key;
		lexicon[i].value = td->lexicon[i].value;
	}

	return td->entry_count;
}
