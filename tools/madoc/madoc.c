/*
This is very rough and built specifically for miniaudio. Don't get clever and try using this for
your own project because it will most likely not work.
*/

#include "../../../fs/fs.c"

#define C89STR_IMPLEMENTATION
#include "../../../c89str/c89str.h"

/*
This runs in two phases. The first phase generates a webplate-compatible template site. The second phase
uses webplate to actually generate the actual website.
*/
#include "../../../webplate/source/libwebplate.c"

/* The output directory, relative to the tools/build/_bin directory. */
#define WEBSITE_DOCS_FOLDER             "website/docs"
#define WEBSITE_DOCS_MANUAL_FOLDER      WEBSITE_DOCS_FOLDER"/manual"
#define WEBSITE_DOCS_EXAMPLES_FOLDER    WEBSITE_DOCS_FOLDER"/examples"
#define WEBSITE_DOCS_API_FOLDER         WEBSITE_DOCS_FOLDER"/api"

#define EXAMPLES_FOLDER                 "examples"



c89str load_file(const char* pFilePath)
{
    fs_result resultFS;
    size_t fileSize;
    char* pFileData;
    c89str str;
    
    resultFS = fs_file_open_and_read(NULL, pFilePath, FS_FORMAT_TEXT, (void**)&pFileData, &fileSize);
    if (resultFS != FS_SUCCESS) {
        printf("Failed to open %s: %s.\n", pFilePath, fs_result_to_string(resultFS));
        return NULL;
    }

    /* We have the file data, but now we need to convert it to a dynamic string so we can manipulate it later. */
    str = c89str_newn(NULL, pFileData, fileSize);
    fs_free(pFileData, NULL);

    return str;
}

int save_file(const char* pFilePath, c89str src)
{
    fs_result resultFS;

    resultFS = fs_file_open_and_write(NULL, pFilePath, src, c89str_len(src));
    if (resultFS != FS_SUCCESS) {
        printf("Failed to save %s: %s.\n", pFilePath, fs_result_to_string(resultFS));
        return (int)resultFS;
    }

    return 0;
}


c89str convert_tabs_to_spaces(c89str str)
{
    /* Assume a tab is 4 spaces. */
    return c89str_replace_all(str, NULL, "\t", 1, "    ", 4);
}


c89str strip_code_block_comments(c89str str)
{
    size_t runningOffset = 0;

    if (str == NULL) {
        return str;
    }

    /*
    Block comments are not recursive which slightly simplifies our algorithm. The algorithm we're using here is not
    based on speed and can be made much more efficient. Note that this will stip blocks that contain an opening or
    closing block within a string constant. The proper way to do this is to use a proper C parser, but that is more
    than what we need right now.
    */
    for (;;) {
        size_t openingOffset;
        size_t closingOffset;

        /* Opening. */
        if (c89str_find(str + runningOffset, "/*", &openingOffset) != C89STR_SUCCESS) {
            break;  /* We're done. */
        }
        openingOffset += runningOffset; /* Normalize the opening offset. */

        /* Closing. */
        if (c89str_find(str + openingOffset, "*/", &closingOffset) != C89STR_SUCCESS) {
            break;  /* We're done. */
        }
        closingOffset += openingOffset; /* Normalize the closing offset. */
        closingOffset += 2;             /* Make sure to include the closing block itself. */

        /* We now have enough information to delete the comment block. */
        str = c89str_replace(str, NULL, openingOffset, (closingOffset - openingOffset), "", 0);
        if (str == NULL) {
            return str;
        }

        /* We're done with this block so we can now move on to the next. */
        runningOffset = openingOffset;
    }

    return str;
}

c89str strip_code_line_comments(c89str str)
{
    if (str == NULL) {
        return str;
    }

    /* TODO: Implement me. */
    return str;
}

c89str strip_code_comments(c89str str)
{
    return strip_code_line_comments(strip_code_block_comments(str));
}



c89str strip_empty_lines(c89str str)
{
    const char* pRunningStr;
    c89str newstr = NULL;
    size_t lineLen;
    size_t nextLineOffset;

    if (str == NULL) {
        return str;
    }

    pRunningStr = str;

    /* We are going to fully recreate the string line by line. We can allocate enough space from the start. */
    newstr = c89str_new_with_cap(NULL, c89str_len(str));
    if (newstr == NULL) {
        return str;
    }

    while (pRunningStr[0] != '\0') {
        nextLineOffset = c89str_utf8_find_next_line(pRunningStr, (size_t)-1, &lineLen);
        
        if (c89str_is_null_or_whitespace(pRunningStr, lineLen) == C89STR_FALSE) {
            newstr = c89str_catn(newstr, NULL, pRunningStr, nextLineOffset);
        }

        if (nextLineOffset == c89str_npos) {
            break;
        } else {
            pRunningStr += nextLineOffset;
        }
    }

    if (newstr == NULL) {
        return str;
    }

    c89str_delete(str, NULL);
    return newstr;
}

c89str strip_trailing_whitespace(c89str str)
{
    const char* pRunningStr;
    c89str newstr = NULL;

    if (str == NULL) {
        return str;
    }

    pRunningStr = str;

    /* We are going to fully recreate the string line by line. We can allocate enough space from the start. */
    newstr = c89str_new_with_cap(NULL, c89str_len(str));
    if (newstr == NULL) {
        return str;
    }

    while (pRunningStr[0] != '\0') {
        size_t lineLen;
        size_t nextLineOffset;

        nextLineOffset = c89str_utf8_find_next_line(pRunningStr, (size_t)-1, &lineLen);
        newstr = c89str_catn(newstr, NULL, pRunningStr, c89str_utf8_rtrim_offset(pRunningStr, lineLen));

        /* Insert the new line character. */
        if (nextLineOffset != c89str_npos) {
            newstr = c89str_catn(newstr, NULL, pRunningStr + lineLen, nextLineOffset - lineLen);
        } else {
            newstr = c89str_catn(newstr, NULL, pRunningStr + lineLen, lineLen);
        }

        if (nextLineOffset == c89str_npos) {
            break;
        } else {
            pRunningStr += nextLineOffset;
        }
    }

    if (newstr == NULL) {
        return str;
    }

    c89str_delete(str, NULL);
    return newstr;
}

c89str strip_whitespace(c89str str)
{
    return strip_trailing_whitespace(strip_empty_lines(str));
}

c89str minify_code(c89str str)
{
    return convert_tabs_to_spaces(strip_whitespace(strip_code_comments(str)));
}




static int path_remove_extension(char* dst, size_t dstSizeInBytes, const char* src)
{
    const char* ext;

    if (src == NULL) {
        if (dst != NULL && dstSizeInBytes > 0) {
            dst[0] = '\0';
        }

        return -1;
    }

    ext = fs_path_extension(src, FS_NULL_TERMINATED);
    if (ext == NULL || ext[0] == '\0') {
        /* No extension. */
        if (dst == src) {
            return (ext - dst);
        } else {
            size_t len = strlen(src);
            fs_strncpy_s(dst, dstSizeInBytes, src, len);

            return (int)len;
        }
    } else {
        /* Have extension. */
        size_t dstLen = (size_t)(ext - src - 1);    /* -1 for the period. */

        if (dst == src) {
            dst[dstLen] = '\0';
        } else {
            fs_strncpy_s(dst, dstSizeInBytes, src, dstLen);
        }

        return dstLen;
    }
}

static int path_append_extension(char* dst, size_t dstSizeInBytes, const char* base, const char* extension)
{
    fs_result result = FS_SUCCESS;
    size_t baseLength;
    size_t extLength;
    size_t dstLen;

    if (base == NULL) {
        base = "";
    }

    if (extension == NULL) {
        extension = "";
    }

    if (extension[0] == '\0') {
        if (dst != NULL) {
            if (dst != base) {
                fs_strcpy_s(dst, dstSizeInBytes, base);
            }
        }
        
        return (int)strlen(base);
    }


    baseLength = strlen(base);
    extLength  = strlen(extension);
    dstLen     = baseLength + 1 + extLength;

    if (dst != NULL) {
        if (dstLen+1 <= dstSizeInBytes) {
            if (dst != base) {
                fs_strcpy_s(dst + 0, dstSizeInBytes - 0, base);
            }
            fs_strcpy_s(dst + baseLength,     dstSizeInBytes - baseLength,     ".");
            fs_strcpy_s(dst + baseLength + 1, dstSizeInBytes - baseLength - 1, extension);
        }
    }

    return (int)dstLen;
}


static fs_result fs_rmdir_content(const char* pDirectory)
{
    /* We'll use an iterator for this. */
    fs_result result;
    fs_iterator* pIterator;

    if (pDirectory == NULL) {
        return FS_INVALID_ARGS;
    }

    pIterator = fs_first(NULL, pDirectory, FS_READ);
    while (pIterator != NULL) {
        char* pFilePath;
        int filePathLen;

        /* Get the length first. */
        filePathLen = fs_path_append(NULL, 0, pDirectory, FS_NULL_TERMINATED, pIterator->pName, pIterator->nameLen);
        if (filePathLen > 0) {
            pFilePath = (char*)fs_malloc((size_t)filePathLen + 1, NULL);    /* +1 for null terminator. */
            if (pFilePath != NULL) {
                fs_path_append(pFilePath, (size_t)filePathLen + 1, pDirectory, FS_NULL_TERMINATED, pIterator->pName, pIterator->nameLen);

                if (pIterator->info.directory) {
                    if (pIterator->pName[0] == '.' && pIterator->pName[1] == '\0') {
                        /* "." - ignore. */
                    } else if (pIterator->pName[0] == '.' && pIterator->pName[1] == '.' && pIterator->pName[2] == '\0') {
                        /* ".." - ignore. */
                    } else {
                        fs_rmdir_content(pFilePath);
                        
                    }
                }

                fs_remove(NULL, pFilePath, 0);
            } else {
                return FS_OUT_OF_MEMORY;
            }
        } else {
            /* Failed to retrieve the file path length. */
        }

        pIterator = fs_next(pIterator);
    }

    fs_free_iterator(pIterator);

    return FS_SUCCESS;
}



typedef struct
{
    c89str name;
    c89str code;
} doc_example;

typedef enum
{
    doc_category_home,
    doc_category_manual,
    doc_category_examples,
    doc_category_api
} doc_category;

typedef enum
{
    doc_token_type_paragraph,
    doc_token_type_code,
    doc_token_type_table,
    doc_token_type_header,
    doc_token_type_list_item
} doc_token_type;

typedef enum
{
    doc_lang_none,
    doc_lang_c
} doc_lang;

typedef struct
{
    const char* pText;
    size_t textLen;     /* Set to (size_t)-1 for null terminated. */
    size_t textOff;     /* The cursor. */
    doc_token_type token;
    const char* pTokenStr;
    size_t tokenLen;
    size_t headerLevel;
    size_t indentation; /* Useful for knowing how to offset code. Code tags can be indented and we want to know how deep the base level of indentation is. */
    size_t prevLineOff;
    size_t prevLineLen;
    doc_lang codeLang;
    int isLastListItem;
    size_t listItemCounter;
} doc_lexer;

int doc_lexer_init(const char* pText, size_t textLen, doc_lexer* pLexer)
{
    if (pText == NULL || pLexer == NULL) {
        return EINVAL;
    }

    C89STR_ZERO_OBJECT(pLexer);

    if (textLen == (size_t)-1) {
        textLen = c89str_strlen(pText);
    }

    pLexer->pText   = pText;
    pLexer->textLen = textLen;
    pLexer->textOff = 0;

    return 0;
}

static int doc_lexer_is_header_underline(const char* pText, size_t textLen, char ch)
{
    size_t i;

    if (textLen == 0) {
        return 0;
    }

    for (i = 0; i < textLen; i += 1) {
        if (pText[i] == '\0') {
            break;
        }

        if (pText[i] != ch) {
            return 0;
        }
    }

    /* Getting here means it's an underline. */
    return 1;
}

static size_t doc_lexer_get_header_underline_level(const char* pText, size_t textLen)
{
    if (doc_lexer_is_header_underline(pText, textLen, '-')) {
        return 2;
    }

    if (doc_lexer_is_header_underline(pText, textLen, '=')) {
        return 1;
    }

    return 0;
}

int doc_lexer_next(doc_lexer* pLexer)
{
    //int result;
    const char* txt;
    size_t off;
    size_t len;

    if (pLexer == NULL) {
        return EINVAL;
    }

    txt = pLexer->pText;
    off = pLexer->textOff;  /* Moves forward. */
    len = pLexer->textLen;  /* Constant. */

    /* We run line-by-line. */
    for (;;) {
        size_t thisLineLen;
        size_t nextLineBeg;
        size_t indentation;
        size_t headerLevel;
        size_t location;    /* When searching for a token on a line. */

        if (txt[off] == '\0' || len == off) {
            /* We're done. The last paragraph needs to be added, if any. */
            if (off > pLexer->textOff) {
                pLexer->token     = doc_token_type_paragraph;
                pLexer->pTokenStr = txt + pLexer->textOff;
                pLexer->tokenLen  = off - pLexer->textOff;
                pLexer->textOff   = off;
                return 0;
            } else {
                return ENOMEM;  /* We're done. */
            }
        }

        /* Grab the next line. */
        nextLineBeg = c89str_utf8_find_next_line(txt + off, len - off, &thisLineLen);

        /* Check if we have a header underline. If so, we want to end the paragraph, if any, and then return. */
        headerLevel = doc_lexer_get_header_underline_level(txt + off, thisLineLen);
        if (headerLevel > 0) {
            if (pLexer->textOff < pLexer->prevLineOff) {
                /* We have a paragraph to post. */
                pLexer->token     = doc_token_type_paragraph;
                pLexer->pTokenStr = txt + pLexer->textOff;
                pLexer->tokenLen  = pLexer->prevLineOff - pLexer->textOff;
                pLexer->textOff   = off;
            } else {
                /* We don't have a paragraph. Just post the header itself. */
                pLexer->token       = doc_token_type_header;
                pLexer->pTokenStr   = txt + pLexer->prevLineOff;
                pLexer->tokenLen    = pLexer->prevLineLen;
                pLexer->textOff     = off + nextLineBeg;
                pLexer->headerLevel = headerLevel;
            }

            return 0;
        }

        /* Getting here means it's not a header. */
        indentation = c89str_utf8_ltrim_offset(txt + off, len - off);

        if (c89str_findn(txt + off, thisLineLen, "```", (size_t)-1, &location) == C89STR_SUCCESS) {
            /* The beginning of a code block. If there's a pending paragraph that needs to be posted. */
            if (pLexer->textOff < off) {
                /* We have a paragraph to post. */
                pLexer->token     = doc_token_type_paragraph;
                pLexer->pTokenStr = txt + pLexer->textOff;
                pLexer->tokenLen  = off - pLexer->textOff;
                pLexer->textOff   = off;
            } else {
                /* We don't have a paragraph. We can now parse the code block. */
                size_t tokenOff = off;
                size_t tokenLen = 0;
                doc_lang lang = doc_lang_none;
                if (txt[off + location + 3] == 'c') {
                    lang = doc_lang_c;
                }

                nextLineBeg = c89str_utf8_find_next_line(txt + off, len - off, &thisLineLen);
                tokenOff = off + nextLineBeg;
                tokenLen = 0;

                off += nextLineBeg;

                /* We now need to keep looping over each line until we find the closing the code block. */
                for (;;) {
                    if (txt[off] == '\0' || off == len) {
                        break;
                    }

                    nextLineBeg = c89str_utf8_find_next_line(txt + off, len - off, &thisLineLen);

                    if (c89str_findn(txt + off, thisLineLen, "```", (size_t)-1, &location) == FS_SUCCESS) {
                        /* We found the end of the code block. */
                        pLexer->token       = doc_token_type_code;
                        pLexer->pTokenStr   = txt + tokenOff;
                        pLexer->tokenLen    = tokenLen;
                        pLexer->textOff     = off + nextLineBeg;
                        pLexer->indentation = indentation;
                        pLexer->codeLang    = lang;
                        break;
                    } else {
                        
                    }

                    off      += nextLineBeg;
                    tokenLen += nextLineBeg;
                }
            }

            return 0;
        }

        /* Getting here means it's not a header nor a code block. It might be a table. */
        if (c89str_findn(txt + off, thisLineLen, "+--", (size_t)-1, &location) == FS_SUCCESS) {
            /* The beginning of a table. The end of the table is the end of the last line that starts with a "+" or "|". */
            if (pLexer->textOff < off) {
                /* We have a paragraph to post. */
                pLexer->token     = doc_token_type_paragraph;
                pLexer->pTokenStr = txt + pLexer->textOff;
                pLexer->tokenLen  = off - pLexer->textOff;
                pLexer->textOff   = off;
            } else {
                /* We don't have a paragraph. We can now parse the table block. */
                size_t tokenOff = off;
                size_t tokenLen = 0;

                for (;;) {
                    if (txt[off] == '\0' || off == len) {
                        break;
                    }

                    nextLineBeg = c89str_utf8_find_next_line(txt + off, len - off, &thisLineLen);

                    if (nextLineBeg < 4 || (txt[off + indentation] != '+' && txt[off + indentation] != '|')) {
                        /* We found the end of the table. The end of the table is the end of this line. */
                        pLexer->token       = doc_token_type_table;
                        pLexer->pTokenStr   = txt + tokenOff;
                        pLexer->tokenLen    = tokenLen;
                        pLexer->textOff     = off + nextLineBeg;
                        pLexer->indentation = indentation;
                        break;
                    } else {
                        /* We're still looking at the table. */
                    }

                    off      += nextLineBeg;
                    tokenLen += nextLineBeg;
                }
            }

            return 0;
        }


        /* Check if it's a bullet point. */
        if (txt[off + indentation] == '-' || txt[off + indentation] == '*') {
            if (pLexer->textOff < off) {
                /* There's a pending paragraph to post. */
                pLexer->token     = doc_token_type_paragraph;
                pLexer->pTokenStr = txt + pLexer->textOff;
                pLexer->tokenLen  = off - pLexer->textOff;
                pLexer->textOff   = off;
            } else {
                /* A bullet point can cover multiple lines. The bullet point ends when a line starts with another bullet point or is empty. */
                size_t tokenOff = off + indentation + 2;
                size_t tokenLen = 0;
                //int isLineBlank = 0;
                int isNextLineBlank = 0;

                off += indentation + 2;

                /* Keep going until we find the start of a new bullet point or an empty line. */
                for (;;) {
                    int foundEndOfBulletPoint = 0;

                    if (txt[off] == '\0' || off == len) {
                        break;  /* At end. */
                    }

                    /* Go to the next line. */
                    nextLineBeg = c89str_utf8_find_next_line(txt + off, len - off, &thisLineLen);

                    /* Determine whether or not the next line is blank. */
                    isNextLineBlank = c89str_is_null_or_whitespace(txt + off + nextLineBeg, c89str_utf8_find_next_line(txt + off + nextLineBeg, len - off - nextLineBeg, NULL));
                    if (isNextLineBlank) {
                        foundEndOfBulletPoint = 1;
                    } else {
                        /* The next line is not blank so now check if it starts with a bullet point. */
                        size_t lineContentOff = off + nextLineBeg + c89str_utf8_ltrim_offset(txt + off + nextLineBeg, len - off - nextLineBeg);
                        if (txt[lineContentOff] == '-' || txt[lineContentOff] == '*' || txt[lineContentOff] == '\0') {
                            foundEndOfBulletPoint = 1;
                        }
                    }

                    if (foundEndOfBulletPoint) {
                        /* We found the end of the bullet point. Update the lexer and return. */
                        pLexer->token       = doc_token_type_list_item;
                        pLexer->pTokenStr   = txt + tokenOff;
                        pLexer->tokenLen    = tokenLen + thisLineLen;
                        pLexer->textOff     = off + nextLineBeg;
                        pLexer->indentation = indentation;

                        /* Reset the list item counter if necessary. */
                        if (pLexer->isLastListItem) {
                            pLexer->listItemCounter = 0;
                        }

                        pLexer->listItemCounter += 1;
                        pLexer->isLastListItem   = isNextLineBlank;

                        return 0;
                    } else {
                        /* We didn't find the end of the bullet point. Move forward. */
                        off      += indentation + nextLineBeg;
                        tokenLen += indentation + nextLineBeg;
                    }
                }
            }

            return 0;
        }


        /* Not a header underline, code block or table. Assume a paragraph. If we have an empty line it means we can terminate our paragraph. */
        if (c89str_is_null_or_whitespace(txt + off, thisLineLen)) {
            if (pLexer->textOff < off) {
                /* We have a paragraph to post. */
                pLexer->token     = doc_token_type_paragraph;
                pLexer->pTokenStr = txt + pLexer->textOff;
                pLexer->tokenLen  = off - pLexer->textOff;
                pLexer->textOff   = off;

                if (!c89str_is_null_or_whitespace(pLexer->pTokenStr, pLexer->tokenLen)) {
                    return 0;
                }
            }
        }


        /* We need to keep track of the previous line for the purpose of header underlines. */
        pLexer->prevLineOff = off;
        pLexer->prevLineLen = thisLineLen;

        /* We're done. We can move to the next line now. */
        off += nextLineBeg;
    }

    /*return 0;*/
}


typedef struct
{
    c89str title;
    c89str bookmark;
    size_t level;
} manual_section;

typedef struct
{
    struct
    {
        c89str raw;
        size_t sectionCount;
        manual_section sections[256];
    } manual;

    struct
    {
        size_t count;
        doc_example examples[256];
    } examples;
} docstate;

c89str extract_manual_section_title(const char* pStr, size_t len, size_t* pLevel)
{
    if (pLevel != NULL) {
        *pLevel = 0;
    }

    /* We want to remove the numbers because we have limited horizontal space. */
    while (pStr[0] != '\0' && len > 0) {
        if ((pStr[0] >= '0' && pStr[0] <= '9') || pStr[0] == '.') {
            pStr += 1;
            len  -= 1;

            if (pStr[0] == '.' && pLevel != NULL) {
                *pLevel += 1;
            }

            continue;
        } else {
            break;
        }
    }

    return c89str_trim(c89str_newn(NULL, pStr, len), NULL);
}

c89str extract_manual_section_bookmark(const char* pStr, size_t len)
{
    return c89str_replace_all(extract_manual_section_title(pStr, len, NULL), NULL, " ", 1, "-", 0);
}

int load_manual(c89str miniaudio, docstate* pState)
{
    int result;
    c89str_lexer lexer;
    int commentCounter;

    result = c89str_lexer_init(&lexer, miniaudio, c89str_len(miniaudio));
    if (result != 0) {
        return result;
    }

    /* The manual will be in the second comment block. The first comment block is the project summary. */
    commentCounter = 0;
    for (;;) {
        result = c89str_lexer_next(&lexer);
        if (result != 0) {
            return result;
        }

        if (lexer.token == c89str_token_type_comment) {
            commentCounter += 1;
        }

        if (commentCounter == 2) {
            /* We've found the comment block with the content of the programming manual. We just need to remove the enveloping comment tokens. */
            c89str_lexer_transform_token(&lexer, &pState->manual.raw, NULL);
            if (pState->manual.raw == NULL) {
                return ENOMEM;
            } else {
                break;
            }
        }
    }

    /* We need to extract all the sections of the manual for the purpose of the navigation panel. */
    {
        doc_lexer docLexer;

        result = doc_lexer_init(pState->manual.raw, c89str_len(pState->manual.raw), &docLexer);
        if (result != 0) {
            return ENOMEM;
        }

        for (;;) {
            result = doc_lexer_next(&docLexer);
            if (result != 0) {
                break;  /* We're done. */
            }

            if (docLexer.token == doc_token_type_header) {
                manual_section section;

                section.title    = extract_manual_section_title(docLexer.pTokenStr, docLexer.tokenLen, &section.level);
                section.bookmark = extract_manual_section_bookmark(docLexer.pTokenStr, docLexer.tokenLen);

                pState->manual.sections[pState->manual.sectionCount] = section;
                pState->manual.sectionCount += 1;
            }
        }
    }

    return 0;
}

int load_examples(docstate* pState)
{
    fs_result resultFS;
    fs_iterator* pIterator;

    /* Extract all examples. */
    pIterator = fs_first(NULL, EXAMPLES_FOLDER, FS_READ);
    while (pIterator != NULL) {
        if (pIterator->info.size > 0) {   /* Ignore any empty files. Sometimes I'll put placeholder files in the examples folder to keep track of ideas for examples. */
            /* We need to get the contents of the file. */
            doc_example example;
            char* pFileData;
            char filePath[4096];

            fs_path_append(filePath, sizeof(filePath), EXAMPLES_FOLDER, FS_NULL_TERMINATED, pIterator->pName, pIterator->nameLen);

            /* Name. We don't care about the whole file path - just the name part of it. */
            example.name = c89str_newn(NULL, pIterator->pName, pIterator->nameLen);

            /* File content. */
            resultFS = fs_file_open_and_read(NULL, filePath, FS_FORMAT_TEXT, (void**)&pFileData, NULL);
            if (resultFS != FS_SUCCESS) {
                printf("Failed to load example: %s\n", pIterator->pName);
                return -1;
            }

            example.code = c89str_new(NULL, pFileData);
            fs_free(pFileData, NULL);

            pState->examples.examples[pState->examples.count] = example;
            pState->examples.count += 1;
        }

        pIterator = fs_next(pIterator);
    }

    fs_free_iterator(pIterator);

    return 0;
}

int load_api(c89str miniaudio, docstate* pState)
{
    (void)miniaudio;
    (void)pState;
    return 0;
}


int load(docstate* pState)
{
    int result;
    c89str miniaudio;

    C89STR_ZERO_OBJECT(pState);

    /* Data is extracted from miniaudio.h, so we'll need to get that loaded as a start. */
    miniaudio = load_file("miniaudio.h");
    if (miniaudio == NULL) {
        printf("Failed to load miniaudio.h");
        return -1;
    }

    result = load_manual(miniaudio, pState);
    if (result != 0) {
        printf("Failed to load manual.");
        return result;
    }

    result = load_examples(pState);
    if (result != 0) {
        printf("Failed to load examples.");
        return result;
    }

    result = load_api(miniaudio, pState);
    if (result != 0) {
        printf("Failed to load API.");
        return result;
    }

    /* We're done. */
    c89str_delete(miniaudio, NULL);
    return 0;
}

c89str raw_to_html(c89str raw);

c89str transform_inline_code(c89str html)
{
    size_t offset = 0;

    /*
    To do this we just need to find the next grave character. Then the content between that and the following grave needs to be wrapped in a <span></span> tag
    with a monospace font.
    */
    for (;;) {
        size_t loc1;
        size_t loc2;
        c89str inner;
        c89str replacement;

        if (c89str_find(html + offset, "`", &loc1) != C89STR_SUCCESS) {
            break;  /* No more occurances. */
        }

        if (c89str_find(html + offset + loc1 + 1, "`", &loc2) != C89STR_SUCCESS) {
            break;  /* No more occurances. */
        }
        loc2 += loc1 + 1;  /* Make the closing grave offset relative to "offset" like loc1. */

        /* We need a copy of the section inside the graves just in case the replacement operation needs to reallocate the pointer. */
        inner = c89str_newn(NULL, html + offset + loc1 + 1, loc2 - loc1 - 1);
        if (inner == NULL) {
            return NULL;    /* Out of memory. */
        }

        replacement = c89str_newf(NULL, "<span style=\"font-family:monospace;\">%s</span>", inner);
        c89str_delete(inner, NULL);

        if (replacement == NULL) {
            return NULL;
        }

        /* We now have what we need to replace the segment. */
        html = c89str_replace(html, NULL, offset + loc1, loc2 - loc1 + 1, replacement, c89str_len(replacement));
        if (html == NULL) {
            c89str_delete(replacement, NULL);
            return NULL;    /* Out of memory. */
        }

        offset += loc1 + c89str_len(replacement);
        c89str_delete(replacement, NULL);
    }

    return html;
}

c89str transform_urls_by_protocol(c89str html, const char* pProtocol)
{
    size_t offset = 0;
    size_t protocolLen;

    protocolLen = c89str_strlen(pProtocol);
    if (protocolLen == 0) {
        return html;
    }

    for (;;) {
        size_t loc1;
        size_t loc2;
        c89str url;
        c89str replacement;

        if (c89str_find(html + offset, pProtocol, &loc1) != C89STR_SUCCESS) {
            break;  /* No more occurances. */
        }

        loc2 = c89str_find_next_whitespace(html + offset + loc1, (size_t)-1, NULL);
        if (loc2 != c89str_npos) {
            loc2 += loc1;
        } else {
            loc2 = c89str_len(html) - offset;
        }

        /* We need a copy of the section inside the graves just in case the replacement operation needs to reallocate the pointer. */
        url = c89str_trim(c89str_newn(NULL, html + offset + loc1, loc2 - loc1), NULL);
        if (url == NULL) {
            return NULL;    /* Out of memory. */
        }

        /* We're going to remove any trailing symbols. */
        size_t len = c89str_len(url);
        while (url[len-1] == '.' || url[len-1] == ')' || url[len-1] == '(' || url[len-1] == ';') {
            len  -= 1;
        }

        url = c89str_setn(url, NULL, url, len);    /* Should never fail since we're just shrinking the string. */
        loc2 = loc1 + len;

        replacement = c89str_newf(NULL, "<a href=\"%s\">%s</a>", url, url);
        c89str_delete(url, NULL);

        if (replacement == NULL) {
            return NULL;
        }

        /* We now have what we need to replace the segment. */
        html = c89str_replace(html, NULL, offset + loc1, loc2 - loc1, replacement, c89str_len(replacement));
        if (html == NULL) {
            c89str_delete(replacement, NULL);
            return NULL;    /* Out of memory. */
        }

        offset += loc1 + c89str_len(replacement);
        c89str_delete(replacement, NULL);
    }

    return html;
}

c89str transform_urls(c89str html)
{
    /* This is similar to inline code segments, except out opening token is "http://" or "https://" and ends with  */
    html = transform_urls_by_protocol(html, "https://");
    html = transform_urls_by_protocol(html, "http://");
    return html;
}

c89str escape_html(const char* pHTML, size_t len)
{
    c89str html = c89str_newn(NULL, pHTML, len);

    /* Slow, but it's simple and it works. */
    html = c89str_replace_all(html, NULL, "&",  (size_t)-1, "&amp;",  (size_t)-1); /* <-- This must come first to ensure it doesn't replace the "&" symbols used in the escapes before it. */
    html = c89str_replace_all(html, NULL, "<",  (size_t)-1, "&lt;",   (size_t)-1);
    html = c89str_replace_all(html, NULL, ">",  (size_t)-1, "&gt;",   (size_t)-1);
    html = c89str_replace_all(html, NULL, "\"", (size_t)-1, "&quot;", (size_t)-1);
    html = c89str_replace_all(html, NULL, "\'", (size_t)-1, "&#39;",  (size_t)-1);

    /* We want to keep <br> tags unescaped. This is not a good way to do this, but it works well enough for now since we won't in practice have this string in our documentation. */
    html = c89str_replace_all(html, NULL, "&lt;br&gt;", (size_t)-1, "<br>", (size_t)-1);

    /* Content inside `` tags need to be formatted as code. */
    html = transform_inline_code(html);

    /* URLs need to be transformed. */
    html = transform_urls(html);

    return html;
        
}

c89str raw_to_html_p(const char* pText, size_t textLen)
{
    c89str p = NULL;

    /* Don't emit anything if the paragraph is empty. */
    if (c89str_is_null_or_whitespace(pText, textLen)) {
        return c89str_new(NULL, "");
    }

    p = c89str_cat(p, NULL, "<p>\n");
    {
        p = c89str_cat(p, NULL, escape_html(pText, textLen));
    }
    p = c89str_cat(p, NULL, "</p>\n");

    return p;
}

const char* g_CKeywords[] = {
    "auto",
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "register",
    "restrict",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
};

/* This is temporary until we get a basic C parser working and we can dynamically extract our custom types. */
const char* g_CustomTypes[] = {
    "ma_uint8",
    "ma_int8",
    "ma_uint16",
    "ma_int16",
    "ma_uint32",
    "ma_int32",
    "ma_uint64",
    "ma_int64",
    "ma_bool8",
    "ma_bool32",
    "ma_mutex",
    "ma_semaphore",
    "ma_event",
    "ma_thread",
    "ma_result",
    "ma_context_config",
    "ma_context",
    "ma_device_config",
    "ma_device",
    "ma_device_info",
    "ma_decoder_config",
    "ma_decoder",
    "ma_encoder_config",
    "ma_encoder",
    "ma_waveform_config",
    "ma_waveform",
    "ma_noise_config",
    "ma_noise",
    "ma_audio_buffer_config",
    "ma_audio_buffer",
    "ma_data_source",
    "ma_pcm_rb",
    "ma_rb",
    "ma_channel_converter_config",
    "ma_channel_converter",
    "ma_resampler_config",
    "ma_resampler",
    "ma_data_converter_config",
    "ma_data_converter",
    "ma_biquad_config",
    "ma_biquad",
    "ma_lpf_config",
    "ma_lpf",
};

#define C_COMMENT_COLOR         "#009900"
#define C_STRING_COLOR          "#cc3300"
#define C_KEYWORD_COLOR         "#0033ff"
#define C_PREPROCESSOR_COLOR    "#666666"
#define C_CUSTOM_TYPE_COLOR     "#0099cc"

c89str_bool32 list_contains_string(const char** ppList, size_t listCount, const char* pText, size_t textLen)
{
    size_t i = 0;
    for (i = 0; i < listCount; i += 1) {
        if (strncmp(ppList[i], pText, textLen) == 0) {
            return C89STR_TRUE;
        }
    }

    return C89STR_FALSE;
}

c89str_bool32 is_c_keyword(const char* pText, size_t textLen)
{
    return list_contains_string(g_CKeywords, C89STR_COUNTOF(g_CKeywords), pText, textLen);
}

c89str_bool32 is_custom_type(const char* pText, size_t textLen)
{
    return list_contains_string(g_CustomTypes, C89STR_COUNTOF(g_CustomTypes), pText, textLen);
}

c89str html_highlight(const char* pText, size_t textLen, const char* pColor)
{
    c89str html = NULL;

    html = c89str_catf(html, NULL, "<span style=\"color:%s\">", pColor);
    html = c89str_cat (html, NULL, escape_html(pText, textLen));
    html = c89str_cat (html, NULL, "</span>");

    return html;
}

c89str raw_to_html_code(const char* pText, size_t textLen, size_t indentation, doc_lang lang)
{
    c89str code = NULL;

    if (textLen == (size_t)-1) {
        textLen = c89str_strlen(pText);
    }

    if (lang == doc_lang_none) {
        code = c89str_cat(code, NULL, "<div style=\"font-family:monospace; margin:1em 0em;\"><pre style=\"margin:0.5em 1em; padding:0; line-height:125%; overflow-x:auto; overflow-y:hidden;\">\n");
    } else {
        code = c89str_cat(code, NULL, "<div style=\"font-family:monospace; border:solid 1px #003800; border-left:solid 0.5em #003800; margin:1em 0em; width:100%;\"><pre style=\"margin:0.5em 1em; padding:0; line-height:125%; overflow-x:auto; overflow-y:hidden;\">\n");
    }
    {
        /* We need to do a quick pre-processing of the string to strip the indentation. Then we need to iterate over each token and reconstruct the string. */
        int result;
        c89str text = NULL;
        const char* pRunningText = pText;
        c89str_lexer lexer;

        while (pRunningText < pText + textLen) {
            size_t thisLineLen;
            size_t nextLineBeg = c89str_utf8_find_next_line(pRunningText, textLen - (pRunningText - pText), &thisLineLen);
        
            if (nextLineBeg > indentation) {
                text = c89str_catn(text, NULL, pRunningText + indentation, nextLineBeg - indentation);
            } else {
                text = c89str_catn(text, NULL, pRunningText, nextLineBeg);
            }

            if (nextLineBeg == thisLineLen) {
                break;
            }

            pRunningText += nextLineBeg;
        }

        /* The code should be stripped of it's indentation so now we need to iterate over each token and construct a html string. */
        result = c89str_lexer_init(&lexer, text, c89str_len(text));
        if (result == 0) {
            while (c89str_lexer_next(&lexer) == 0) {
                if (lexer.token == c89str_token_type_comment) {
                    code = c89str_cat(code, NULL, html_highlight(lexer.pTokenStr, lexer.tokenLen, C_COMMENT_COLOR));
                } else if (lexer.token == c89str_token_type_string_double || lexer.token == c89str_token_type_string_single) {
                    code = c89str_cat(code, NULL, html_highlight(lexer.pTokenStr, lexer.tokenLen, C_STRING_COLOR));
                } else {
                    /* Special case if we're handling a pre-processor keyword. We want to get the next identifier and highlight the entire segment appropriately. */
                    if (lexer.token == '#') {
                        const char* pSegmentStr = lexer.pTokenStr;
                        while (c89str_lexer_next(&lexer) == 0) {
                            if (lexer.token == c89str_token_type_eof) {
                                break;
                            }

                            if (lexer.token == c89str_token_type_identifier) {
                                code = c89str_cat(code, NULL, html_highlight(pSegmentStr, (lexer.pTokenStr - pSegmentStr) + lexer.tokenLen, C_PREPROCESSOR_COLOR));

                                if (strncmp(lexer.pTokenStr, "include", lexer.tokenLen) == 0) {
                                    while (c89str_lexer_next(&lexer) == 0 && lexer.token == c89str_token_type_whitespace) {
                                        code = c89str_cat(code, NULL, escape_html(lexer.pTokenStr, lexer.tokenLen));
                                    }

                                    /* If the token is "<" we will want to make sure we highlight it as a string. */
                                    if (lexer.token == c89str_token_type_string_double || lexer.token == c89str_token_type_string_single) {
                                        code = c89str_cat(code, NULL, html_highlight(lexer.pTokenStr, lexer.tokenLen, C_STRING_COLOR));
                                    } else {
                                        if (lexer.token == '<') {
                                            /* We're highligting an #include. */
                                            const char* pIncludeStr = lexer.pTokenStr;
                                            while (c89str_lexer_next(&lexer) == 0 && lexer.token != '>') {
                                                if (lexer.token == c89str_token_type_eof) {
                                                    break;
                                                }
                                            }

                                            code = c89str_cat(code, NULL, html_highlight(pIncludeStr, (lexer.pTokenStr - pIncludeStr) + lexer.tokenLen, C_STRING_COLOR));
                                        }
                                    }
                                }

                                break;
                            }
                        }
                    } else {
                        if (lang == doc_lang_c) {
                            if (is_c_keyword(lexer.pTokenStr, lexer.tokenLen)) {
                                code = c89str_cat(code, NULL, html_highlight(lexer.pTokenStr, lexer.tokenLen, C_KEYWORD_COLOR));
                            } else if (is_custom_type(lexer.pTokenStr, lexer.tokenLen)) {
                                code = c89str_cat(code, NULL, html_highlight(lexer.pTokenStr, lexer.tokenLen, C_CUSTOM_TYPE_COLOR));
                            } else {
                                code = c89str_cat(code, NULL, escape_html(lexer.pTokenStr, lexer.tokenLen));
                            }
                        } else {
                            code = c89str_cat(code, NULL, escape_html(lexer.pTokenStr, lexer.tokenLen));
                        }
                    }
                }
            }
        }
    }
    code = c89str_cat(code, NULL, "</pre></div>");

    return code;
}


/* We never have huge tables in our documentation. */
#define MAX_TABLE_COLUMNS   16
#define MAX_TABLE_ROWS      128

typedef struct
{
    size_t cellCount;
    c89str cells[MAX_TABLE_COLUMNS];
} table_row;

typedef struct
{
    size_t colCount;
    size_t rowCount;
    table_row rows[MAX_TABLE_ROWS]; /* First row is the header. */
} table;

void merge_table_row(table_row* pTarget, const table_row* pSource)
{
    size_t cellCount;
    size_t iCell;

    /* This assumes the cell count is the same. */
    cellCount = pTarget->cellCount;

    for (iCell = 0; iCell < cellCount; iCell += 1) {
        pTarget->cells[iCell] = c89str_catf(pTarget->cells[iCell], NULL, "\n%s", pSource->cells[iCell]);
    }
}

table_row parse_table_row(c89str line)
{
    table_row row;
    const char* pRunningStr = line;

    C89STR_ZERO_OBJECT(&row);

    /* All we're doing is splitting on '|'. */
    pRunningStr += c89str_utf8_ltrim_offset(line, c89str_len(line)) + 1;   /* Skip the initial '|' character. */

    for (;;) {
        size_t loc;

        if (c89str_find(pRunningStr, "|", &loc) != FS_SUCCESS) {
            break;
        }

        row.cells[row.cellCount] = c89str_trim(c89str_newn(NULL, pRunningStr, loc), NULL);
        row.cellCount += 1;

        pRunningStr += loc + 1; /* Plus one for the pipe character. */
    }

    return row;
}

table parse_table(const char* pText, size_t textLen)
{
    table t;
    const char* pRunningText = pText;
    size_t thisLineLen;
    size_t nextLineBeg;

    C89STR_ZERO_OBJECT(&t);

    /* We just ignore the first row which should be starting with "+". */
    pRunningText += c89str_utf8_find_next_line(pRunningText, textLen, NULL);

    /*
    There's two ways to generate define the rows in a table. The first is just one line equals one row. The other uses a separator to define the rows. To distinguish
    between the two we just need to count how many separators there are in the table. If there is 3, we can use the one line technique.
    */
    while (pRunningText < pText + textLen) {
        c89str line;
        size_t loff;

        nextLineBeg = c89str_utf8_find_next_line(pRunningText, textLen - (pRunningText - pText), &thisLineLen);

        line = c89str_newn(NULL, pRunningText, nextLineBeg);   /* Intentionally using nextLineBeg instead of thisLineEnd so we can capture the new-line character. */
        loff = c89str_utf8_ltrim_offset(line, c89str_len(line));

        if (line[loff] == '|') {
            table_row row = parse_table_row(line);

            if (c89str_is_null_or_whitespace(row.cells[0], (size_t)-1)) {
                /* There's nothing in the first cell so we're just going to merge the rows. */
                merge_table_row(&t.rows[t.rowCount], &row);
            } else {
                /* It's a new row. Anything in the current row needs to be committed, and then the new row started. */
                if (!c89str_is_null_or_whitespace(t.rows[t.rowCount].cells[0], (size_t)-1)) {
                    t.rowCount += 1;    /* Commit the existing row if there's anything there at the moment. */
                }
                t.rows[t.rowCount] = row;
            }
        } else if (line[loff] == '+') {
            t.rowCount += 1;    /* Commit the row. */
        }

        pRunningText += nextLineBeg;
    }

    /* The column count can be set to the cell count of the first row. */
    if (t.rowCount > 0) {
        t.colCount = t.rows[0].cellCount;
    }

    return t;
}

c89str raw_to_html_table(const char* pText, size_t textLen, size_t indentation)
{
    c89str html = NULL;

    (void)indentation;

    html = c89str_cat(html, NULL, "<div style=\"overflow:hidden;\"><table class=\"doc\">");
    {
        /* We're doing to generate the table in two passes. The first is going to extract the contents of the table, the second will generate the HTML. */
        table t;
        size_t iRow;
        size_t iCol;

        /* First step is to parse the table. */
        t = parse_table(pText, textLen);

        /* Now that we have the table we can generate the HTML. */
        for (iRow = 0; iRow < t.rowCount; iRow += 1) {
            html = c89str_cat(html, NULL, "<tr>\n");
            {
                for (iCol = 0; iCol < t.colCount; iCol += 1) {
                    html = c89str_catf(html, NULL, "<t%s class=\"doc\" valign=\"top\">", (iRow == 0) ? "h" : "d");
                    html = c89str_cat(html, NULL, raw_to_html(t.rows[iRow].cells[iCol]));
                    html = c89str_catf(html, NULL, "</t%s>\n", (iRow == 0) ? "h" : "d");
                }
            }
            html = c89str_cat(html, NULL, "</tr>\n");
        }
    }
    html = c89str_cat(html, NULL, "</table></div>");

    return html;
}

c89str raw_to_html_header(const char* pText, size_t textLen, size_t headerLevel)
{
    c89str header = NULL;

    header = c89str_catf(header, NULL, "<h%d id=\"%s\" class=\"man\">", (int)headerLevel, extract_manual_section_bookmark(pText, textLen));
    {
        header = c89str_catn(header, NULL, pText, textLen);
    }
    header = c89str_catf(header, NULL, "</h%d>\n", (int)headerLevel);

    return header;
}

c89str raw_to_html_list_item(const char* pText, size_t textLen, size_t itemCounter, int isLastItem)
{
    c89str html = NULL;

    if (itemCounter == 1) {
        html = c89str_cat(html, NULL, "<ul style=\"overflow:hidden;\">\n");
    }

    html = c89str_cat(html, NULL, "<li>\n");
    {
        html = c89str_cat(html, NULL, escape_html(pText, textLen));
    }
    html = c89str_cat(html, NULL, "</li>\n");

    if (isLastItem) {
        html = c89str_cat(html, NULL, "</ul>\n");
    }

    return html;
}

c89str raw_to_html(c89str raw)
{
    int result;
    c89str html = NULL;
    doc_lexer lexer;

    result = doc_lexer_init(raw, c89str_len(raw), &lexer);
    if (result != 0) {
        return NULL;
    }

    for (;;) {
        result = doc_lexer_next(&lexer);
        if (result != 0) {
            break;  /* We're done. */
        }

        if (lexer.token == doc_token_type_header) {
            html = c89str_cat(html, NULL, raw_to_html_header(lexer.pTokenStr, lexer.tokenLen, lexer.headerLevel));
        } else if (lexer.token == doc_token_type_code) {
            html = c89str_cat(html, NULL, raw_to_html_code(lexer.pTokenStr, lexer.tokenLen, lexer.indentation, lexer.codeLang));
        } else if (lexer.token == doc_token_type_table) {
            html = c89str_cat(html, NULL, raw_to_html_table(lexer.pTokenStr, lexer.tokenLen, lexer.indentation));
        } else if (lexer.token == doc_token_type_list_item) {
            html = c89str_cat(html, NULL, raw_to_html_list_item(lexer.pTokenStr, lexer.tokenLen, lexer.listItemCounter, lexer.isLastListItem));
        } else {
            html = c89str_cat(html, NULL, raw_to_html_p(lexer.pTokenStr, lexer.tokenLen));
        }
    }

    return html;
}

c89str example_name_to_display(c89str name)
{
    char* pRunningStr;
    c89str display = NULL;
    char temp[256];
    path_remove_extension(temp, sizeof(temp), name);

    display = c89str_new(NULL, temp);
    display = c89str_replace_all(display, NULL, "_", (size_t)-1, " ", (size_t)-1);

    /* We need to capitalize the first character of each word. */
    if (display[0] >= 'a' && display[0] <= 'z') {
        display[0] -= 32;
    }

    /* For each remaining word. */
    pRunningStr = display;
    for (;;) {
        size_t next = c89str_find_next_whitespace(pRunningStr, (size_t)-1, NULL);
        if (next == c89str_npos) {
            break;  /* We're done. */
        }

        if (pRunningStr[next+1] >= 'a' && pRunningStr[next+1] <= 'z') {
            pRunningStr[next+1] -= 32;
        }

        pRunningStr += next + 1;
    }

    return display;
}

c89str example_name_to_html_file_name(c89str name)
{
    char html[4096];

    path_remove_extension(html, sizeof(html), name);
    path_append_extension(html, sizeof(html), html, "html");

    return c89str_new(NULL, html);
}

c89str extract_example_summary_from_comment(c89str comment)
{
    int result;
    doc_lexer lexer;

    result = doc_lexer_init(comment, c89str_len(comment), &lexer);
    if (result != 0) {
        return c89str_new(NULL, "");    /* Failed to initialize lexer. */
    }

    /* All we need to do is extract the first paragraph. */
    result = doc_lexer_next(&lexer);
    if (result != 0) {
        return c89str_new(NULL, "");
    }

    return c89str_newn(NULL, lexer.pTokenStr, lexer.tokenLen);
}

c89str extract_example_summary(c89str code)
{
    int result;
    c89str_lexer lexer;
    c89str comment;
    c89str summary;

    /* The summary is the first paragraph of the top section of the code which will be in a block comment. */
    result = c89str_lexer_init(&lexer, code, c89str_len(code));
    if (result != 0) {
        return c89str_new(NULL, "");    /* Failed. */
    }

    /*
    We have the C lexer ready to go. We need to get the first block comment. To do this we just exclude whitespace and new lines and take the first
    token. If it's a comment, that'll be where we draw the summary from. Otherwise we'll just return an empty string.
    */
    lexer.options.skipNewlines   = C89STR_TRUE;
    lexer.options.skipWhitespace = C89STR_TRUE;

    result = c89str_lexer_next(&lexer);
    if (result != 0 || lexer.token != c89str_token_type_comment) {
        return c89str_new(NULL, "");    /* Failed to retrieve the first token. */
    }

    /* We now want to format the comment in preparation for running it through the documentation lexer. */
    result = c89str_lexer_transform_token(&lexer, &comment, NULL);
    if (result != 0) {
        return c89str_new(NULL, "");    /* Failed to transform comment. */
    }

    /* Trim the comment to ensure all leading whitespace and new line characters are excluded. */
    comment = c89str_trim(comment, NULL);

    /* We now have enough information to extract the summary from the comment. */
    summary = c89str_trim(extract_example_summary_from_comment(comment), NULL);
    c89str_delete(comment, NULL);

    return summary;
}

c89str navigation_to_html(docstate* pState, doc_category category, const char* pEntityName)
{
    c89str html = NULL;

    (void)pState;

    if (category == doc_category_home) {
        html = c89str_cat(html, NULL, "<a href=\"{{ relative-path \"docs/index.html\" }}\" class=\"doc-navigation doc-navigation-active\">Documentation Home</a>");
    } else {
        html = c89str_cat(html, NULL, "<a href=\"{{ relative-path \"docs/index.html\" }}\" class=\"doc-navigation\">Documentation Home</a>");
    }

    if (category == doc_category_manual) {
        size_t iSection;

        html = c89str_cat(html, NULL, "<a href=\"{{ relative-path \"docs/manual/index.html\" }}\" class=\"doc-navigation doc-navigation-active\">Programming Manual</a>");

        for (iSection = 0; iSection < pState->manual.sectionCount; iSection += 1) {
            if (pState->manual.sections[iSection].level == 1) {
                html = c89str_catf(html, NULL, "<a href=\"#%s\" class=\"doc-navigation doc-navigation-l%d\">%s</a>", pState->manual.sections[iSection].bookmark, (int)pState->manual.sections[iSection].level, pState->manual.sections[iSection].title);
            }
        }
    } else {
        html = c89str_cat(html, NULL, "<a href=\"{{ relative-path \"docs/manual/index.html\" }}\" class=\"doc-navigation\">Programming Manual</a>");
    }
    
    if (category == doc_category_examples) {
        size_t iExample;

        html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"./docs/examples/index.html\" }}\" class=\"doc-navigation %s\">Examples</a>", (pEntityName == NULL) ? "doc-navigation-active" : "");

        for (iExample = 0; iExample < pState->examples.count; iExample += 1) {
            c89str_bool32 isActive = C89STR_FALSE;
            if (pEntityName != NULL && strcmp(pEntityName, pState->examples.examples[iExample].name) == 0) {
                isActive = C89STR_TRUE;
            }

            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/examples/%s\" }}\" class=\"doc-navigation doc-navigation-l%d %s\">%s</a>", example_name_to_html_file_name(pState->examples.examples[iExample].name), (int)1, (isActive) ? "doc-navigation-active" : "", example_name_to_display(pState->examples.examples[iExample].name));
        }
    } else {
        html = c89str_cat(html, NULL, "<a href=\"{{ relative-path \"docs/examples/index.html\" }}\" class=\"doc-navigation\">Examples</a>");
    }

    if (category == doc_category_api) {
        html = c89str_cat(html, NULL, "<a href=\"{{ relative-path \"docs/api/index.html\" }}\" class=\"doc-navigation doc-navigation-active\">API Reference</a>");
    } else {
        html = c89str_cat(html, NULL, "<a href=\"{{ relative-path \"docs/api/index.html\" }}\" class=\"doc-navigation\" style=\"border-bottom:none;\">API Reference</a>");
    }

    return html;
}

c89str generate_html_generic(docstate* pState, const char* pNavitationHTML, const char* pBodyHTML)
{
    c89str html = NULL;

    (void)pState;

    html = c89str_cat(html, NULL, "{{ miniaudio-header }}\n");
    {
        html = c89str_cat(html, NULL, "<table border=\"0\" style=\"margin:0 auto; width:100%; border-collapse:collapse; border:solid 0px #000; table-layout:fixed;\"><tr>\n");
        {
            /* Navigation panel. */
            html = c89str_cat(html, NULL, "<td valign=\"top\" style=\"width:20em; padding:0; margin:0; border-right:solid 0px #000;\"><div style=\"position:relative; height:100%; width:100%; border:solid 0px #000; padding:0; margin:0;\">\n");
            {
                html = c89str_cat(html, NULL, pNavitationHTML);
            }
            html = c89str_cat(html, NULL, "</div></td>");

            /* Body. */
            html = c89str_cat(html, NULL, "<td valign=\"top\" style=\"padding:1em; border-left:solid 1px #bbb;\">\n");
            {
                html = c89str_cat(html, NULL, pBodyHTML);
            }
            html = c89str_cat(html, NULL, "</td>");
        }
        html = c89str_cat(html, NULL, "\n</tr></table>");
    }
    html = c89str_cat(html, NULL, "\n{{ miniaudio-footer }}");

    return html;
}


static const char* g_HTMLBannerImage = "<div style=\"text-align:center; overflow:hidden;\"><img src=\"{{ relative-path \"img/logo1_large.png\" }}\" style=\"width:auto; height:auto; min-height:70px; overflow:hidden;\"></div>";

c89str generate_home_index_html(docstate* pState)
{
    c89str html = NULL;

    (void)pState;

    html = c89str_cat(html, NULL, "<div style=\"text-align:center; padding:1em;\">");
    {
        html = c89str_cat(html, NULL, g_HTMLBannerImage);
        html = c89str_cat(html, NULL, "<div style=\"padding-top:1em; font-weight:bold; font-size:2em; color:#444;\">Documentation</div>");
        html = c89str_cat(html, NULL, "<div style=\"padding-top:0.75em; text-align:center;\">");
        {
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/manual/index.html\" }}\">Programming Manual</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/examples/index.html\" }}\">Examples</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/api/index.html\" }}\">API Reference</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"https://github.com/mackron/miniaudio\">Source Code</a>");
        }
        html = c89str_cat(html, NULL, "</div>");
    }
    html = c89str_cat(html, NULL, "</div>");

    return html;
}

c89str generate_home_html(docstate* pState)
{
    return generate_html_generic(pState, navigation_to_html(pState, doc_category_home, NULL), generate_home_index_html(pState));
}


c89str generate_manual_html(docstate* pState)
{
    c89str html = NULL;

    html = c89str_cat(html, NULL, "<div style=\"text-align:center; padding:1em; padding-bottom:2em;\">");
    {
        html = c89str_cat(html, NULL, g_HTMLBannerImage);
        html = c89str_cat(html, NULL, "<div style=\"padding-top:1em; font-weight:bold; font-size:2em; color:#444;\">Programming Manual</div>");
        html = c89str_cat(html, NULL, "<div style=\"padding-top:0.75em; text-align:center;\">");
        {
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/examples/index.html\" }}\">Examples</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/api/index.html\" }}\">API Reference</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"https://github.com/mackron/miniaudio\">Source Code</a>");
        }
        html = c89str_cat(html, NULL, "</div>");
    }
    html = c89str_cat(html, NULL, "</div>\n");

    html = c89str_cat(html, NULL, raw_to_html(pState->manual.raw));

    return generate_html_generic(pState, navigation_to_html(pState, doc_category_manual, NULL), html);
}


c89str generate_example_index_body(docstate* pState)
{
    c89str html = NULL;

    html = c89str_cat(html, NULL, "<div style=\"text-align:center; padding:1em; padding-bottom:2em;\">");
    {
        html = c89str_cat(html, NULL, g_HTMLBannerImage);
        html = c89str_cat(html, NULL, "<div style=\"padding-top:1em; font-weight:bold; font-size:2em; color:#444;\">Examples</div>");
        html = c89str_cat(html, NULL, "<div style=\"padding-top:0.75em; text-align:center;\">");
        {
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/manual/index.html\" }}\">Programming Manual</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/api/index.html\" }}\">API Reference</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"https://github.com/mackron/miniaudio\">Source Code</a>");
        }
        html = c89str_cat(html, NULL, "</div>");
    }
    html = c89str_cat(html, NULL, "</div>\n");

    /* At the moment this is just a list of examples. */
    html = c89str_cat(html, NULL, "<table style=\"border:none;\">");
    {
        size_t iExample;
        for (iExample = 0; iExample < pState->examples.count; iExample += 1) {
            html = c89str_cat(html, NULL, "<tr>");
            {
                /* Name and URL. */
                html = c89str_cat(html, NULL, "<td style=\"padding-right:2em;\">");
                {
                    html = c89str_catf(html, NULL, "<a href=\"%s\">%s</a>", example_name_to_html_file_name(pState->examples.examples[iExample].name), example_name_to_display(pState->examples.examples[iExample].name));
                }
                html = c89str_cat(html, NULL, "</td>");

                /* Summary. */
                html = c89str_cat(html, NULL, "<td>");
                {
                    html = c89str_cat(html, NULL, extract_example_summary(pState->examples.examples[iExample].code)); // example_name_to_display(pState->examples.examples[iExample].name));   /* TODO: Change this to the summary. */
                }
                html = c89str_cat(html, NULL, "</td>");
            }
            html = c89str_cat(html, NULL, "</tr>");
        }
    }
    html = c89str_cat(html, NULL, "</table>");

    return html;
}

c89str generate_example_index_html(docstate* pState)
{
    return generate_html_generic(pState, navigation_to_html(pState, doc_category_examples, NULL), generate_example_index_body(pState));
}

c89str generate_example_html_body(docstate* pState, const doc_example* pExample)
{
    c89str html = NULL;
    c89str_lexer lexer;
    const char* pCodeStart = pExample->code;

    (void)pState;

    html = c89str_catf(html, NULL, "<h1>%s</h1>", example_name_to_display(pExample->name));
    {
        /* We use a C lexer to extract the top section which is in a comment. We then convert this to HTML. */
        int result = c89str_lexer_init(&lexer, pExample->code, c89str_len(pExample->code));
        if (result == 0) {
            c89str comment;

            lexer.options.skipWhitespace = C89STR_TRUE;
            lexer.options.skipNewlines   = C89STR_TRUE;

            result = c89str_lexer_next(&lexer);
            if (result == 0 && lexer.token == c89str_token_type_comment) {
                result = c89str_lexer_transform_token(&lexer, &comment, NULL);
                if (result == 0) {
                    comment = c89str_trim(comment, NULL);
                    html = c89str_cat(html, NULL, raw_to_html(comment));
                    c89str_delete(comment, NULL);

                    pCodeStart = lexer.pText + lexer.textOff;
                }    
            }
        }
    }
    html = c89str_cat(html, NULL, raw_to_html_code(pCodeStart + c89str_utf8_ltrim_offset(pCodeStart, (size_t)-1), (size_t)-1, 0, doc_lang_c));

    return html;
}

c89str generate_example_html(docstate* pState, const doc_example* pExample)
{
    c89str body = NULL;
    c89str html = NULL;

    body = generate_example_html_body(pState, pExample);

    html = generate_html_generic(pState, navigation_to_html(pState, doc_category_examples, pExample->name), body);
    c89str_delete(body, NULL);

    return html;
}

int generate_examples(docstate* pState)
{
    int result;
    size_t iExample;

    /* Examples home page. This is basically just a list of examples. */
    result = save_file(WEBSITE_DOCS_EXAMPLES_FOLDER"/index.html", generate_example_index_html(pState));
    if (result != 0) {
        return result;  /* Failed to output the examples index page. */
    }

    for (iExample = 0; iExample < pState->examples.count; iExample += 1) {
        /* The file path is the same as the file name, only with .html as the extension. */
        char filePath[4096];
        
        fs_path_append       (filePath, sizeof(filePath), WEBSITE_DOCS_EXAMPLES_FOLDER, FS_NULL_TERMINATED, pState->examples.examples[iExample].name, FS_NULL_TERMINATED);
        path_remove_extension(filePath, sizeof(filePath), filePath);
        path_append_extension(filePath, sizeof(filePath), filePath, "html");

        result = save_file(filePath, generate_example_html(pState, &pState->examples.examples[iExample]));
        if (result != 0) {
            return result;  /* Failed to save the file. */
        }
    }

    return 0;
}


c89str generate_api_index_body(docstate* pState)
{
    c89str html = NULL;

    (void)pState;

    html = c89str_cat(html, NULL, "<div style=\"text-align:center; padding:1em; padding-bottom:2em;\">");
    {
        html = c89str_cat(html, NULL, g_HTMLBannerImage);
        html = c89str_cat(html, NULL, "<div style=\"padding-top:1em; font-weight:bold; font-size:2em; color:#444;\">API Reference</div>");
        html = c89str_cat(html, NULL, "<div style=\"padding-top:0.75em; text-align:center;\">");
        {
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/manual/index.html\" }}\">Programming Manual</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"{{ relative-path \"docs/examples/index.html\" }}\">Examples</a> - ");
            html = c89str_catf(html, NULL, "<a href=\"https://github.com/mackron/miniaudio\">Source Code</a>");
        }
        html = c89str_cat(html, NULL, "</div>");
    }
    html = c89str_cat(html, NULL, "</div>\n");

    html = c89str_cat(html, NULL, "Coming soon...");

    return html;
}

c89str generate_api_index_html(docstate* pState)
{
    return generate_html_generic(pState, navigation_to_html(pState, doc_category_api, NULL), generate_api_index_body(pState));
}

int generate_api(docstate* pState)
{
    int result;

    /* API home page. This is basically just a list of examples. */
    result = save_file(WEBSITE_DOCS_API_FOLDER"/index.html", generate_api_index_html(pState));
    if (result != 0) {
        return result;  /* Failed to output the examples index page. */
    }

    /* TODO: Generate API files. */

    return 0;
}


int generate(docstate* pState)
{
    int result;


    fs_rmdir_content(WEBSITE_DOCS_FOLDER);


    /* Home */
    result = save_file(WEBSITE_DOCS_FOLDER"/index.html", generate_home_html(pState));
    if (result != 0) {
        return result;
    }


    /* Manual */
    result = save_file(WEBSITE_DOCS_MANUAL_FOLDER"/index.html", generate_manual_html(pState));
    if (result != 0) {
        return result;
    }


    /* Examples */
    result = generate_examples(pState);
    if (result != 0) {
        return result;
    }


    /* API */
    result = generate_api(pState);
    if (result != 0) {
        return result;
    }


    /* We're done. */
    return 0;
}


int main(int argc, char** argv)
{
    int result;
    docstate state;

    (void)argc;
    (void)argv;

    result = load(&state);
    if (result != 0) {
        return result;
    }

    /*
    NOTE:

    There's a weird permission error going on with my NAS. For now, you need to generate the website separately,
    and then generate the output via webplate as a separate step. In addition, clearing the output directory with
    WEBPLATE_FLAG_CLEAR_OUTDIR will sometimes fail. To address this, manually delete the output directory and run
    webplate again.
    */
    #if 1
    {
        result = generate(&state);
        if (result != 0) {
            return result;
        }
    }
    #endif

    /* Generate the final website with webplate. */
    if (webplate_process("website", "../miniaud.io", WEBPLATE_FLAG_CLEAR_OUTDIR | 0) != WEBPLATE_SUCCESS) {
        printf("Failed to generate website via webplate.");
    }

    return 0;
}
