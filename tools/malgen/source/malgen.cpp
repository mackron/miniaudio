
#define DR_IMPLEMENTATION
#include "../../../../../dr_libs/dr.h"

#include <vector>

typedef struct
{
    const char* name;
    const char* params[4];
    int paramCount;
} malgen_instruction;

typedef struct
{
    const char* formatInStr;
    const char* formatOutStr;
    std::vector<malgen_instruction> instructions;
} malgen_conversion_desc;

typedef struct
{
    char* pFormatsFileData;
    std::vector<malgen_conversion_desc> conversions;
} malgen_context;

void u8_to_s16(const unsigned char* px, short* pr, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = px[i];

        r = x - 128;
        r = x << 8;
        pr[i] = r;
    }
}

void u8_to_s24(const unsigned char* px, unsigned char* pr, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = px[i];

        r = x - 128;
        r = x << 16;
        memcpy(&pr[i*3], &r, 3);
    }
}

void u8_to_s32(const unsigned char* px, int* pr, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = px[i];

        r = x - 128;
        r = x << 24;
        pr[i] = r;
    }
}

void u8_to_f32(const unsigned char* pIn, float* pOut, unsigned int count)
{
    float r;
    int   x;
    unsigned int i = 0;
    
    switch ((uintptr_t)pIn & 0x3)
    {
    case 3:
        x = pIn[i];
        r = x / 255.0f;
        r = r * 2;
        r = r - 1;
        pOut[i++] = r;
    case 2:
        x = pIn[i];
        r = x / 255.0f;
        r = r * 2;
        r = r - 1;
        pOut[i++] = r;
    case 1:
        x = pIn[i];
        r = x / 255.0f;
        r = r * 2;
        r = r - 1;
        pOut[i++] = r;
    }

    while (i+3 < count) {
        float r[4];
        int   x[4];
        
        x[0] = pIn[i+0];
        x[1] = pIn[i+1];
        x[2] = pIn[i+2];
        x[3] = pIn[i+3];

        r[0] = x[0] / 255.0f;
        r[1] = x[1] / 255.0f;
        r[2] = x[2] / 255.0f;
        r[3] = x[3] / 255.0f;

        r[0] = r[0] * 2;
        r[1] = r[1] * 2;
        r[2] = r[2] * 2;
        r[3] = r[3] * 2;

        r[0] = r[0] - 1;
        r[1] = r[1] - 1;
        r[2] = r[2] - 1;
        r[3] = r[3] - 1;

        pOut[i+0] = r[0];
        pOut[i+1] = r[1];
        pOut[i+2] = r[2];
        pOut[i+3] = r[3];

        i += 4;
    }

    switch (count - i)
    {
    case 3:
        x = pIn[i];
        r = x / 255.0f;
        r = r * 2;
        r = r - 1;
        pOut[i++] = r;
    case 2:
        x = pIn[i];
        r = x / 255.0f;
        r = r * 2;
        r = r - 1;
        pOut[i++] = r;
    case 1:
        x = pIn[i];
        r = x / 255.0f;
        r = r * 2;
        r = r - 1;
        pOut[i++] = r;
    }
}

int malgen_compile(malgen_context* pContext)
{
    size_t formatsFileData;
    pContext->pFormatsFileData = dr_open_and_read_text_file("../../../resources/format_conversions.txt", &formatsFileData);
    if (pContext->pFormatsFileData == NULL) {
        printf("Failed to open sample format conversion definitions.\n");
        return -1;
    }

    // The first part is going to blank out comments. It just makes the actual parsing soooo much simpler.
    char* pRunningStr = pContext->pFormatsFileData;
    for (;;) {
        if (pRunningStr[0] == '\0') break;
        if (pRunningStr[0] == '#') {
            while (pRunningStr[0] != '\n') {
                if (pRunningStr[0] == '\0') {
                    break;
                }

                pRunningStr[0] = ' ';
                pRunningStr += 1;
            }
        }
        pRunningStr += 1;
    }


    malgen_conversion_desc currentFunc;
    malgen_instruction currentInst;
    memset(&currentInst, 0, sizeof(currentInst));
    int currentParam = 0;
    
    // Level 0 = not inside a conversion function.
    // Level 1 = inside a conversion function.
    // Level 2 = inside an instruction (parsing parameters).
    int level = 0;

    pRunningStr = pContext->pFormatsFileData;
    for (;;) {
        if (pRunningStr[0] == '\0')
            return 0;

        if (level == 0) {
            // Find the first non-whitespace.
            for (;;) {
                if (pRunningStr[0] == '\0') return 0;
                if (!dr_is_whitespace(pRunningStr[0])) {
                    break;
                }

                pRunningStr += 1;
            }

            // Find the end of the conversion name.
            char* pConversionNameBeg = pRunningStr;
            char* pConversionNameEnd = pConversionNameBeg;
            for (;;) {
                if (pConversionNameEnd[0] == '\0') return -1;
                if (dr_is_whitespace(pConversionNameEnd[0])) {
                    break;
                }

                pConversionNameEnd += 1;
            }

            pConversionNameEnd[0] = '\0';
            pRunningStr = pConversionNameEnd;
            pRunningStr += 1;

            // Find the opening bracket.
            for (;;) {
                if (pRunningStr[0] == '\0') return -1;
                if (pRunningStr[0] == '{') {
                    currentFunc.formatInStr = pConversionNameBeg;
                    char* formatInStrEnd = strstr((char*const)currentFunc.formatInStr, "->");
                    formatInStrEnd[0] = '\0';
                    currentFunc.formatOutStr = formatInStrEnd += 2;

                    level += 1;
                    break;
                }

                pRunningStr += 1;
            }
        } else if (level == 1) {
            // Inside a function.

            // Find the first non-whitespace which is where the first instruction should be located.
            for (;;) {
                if (pRunningStr[0] == '\0') return -1;
                if (!dr_is_whitespace(pRunningStr[0])) {
                    break;
                }

                pRunningStr += 1;
            }

            // Should be sitting at the name of the next instruction or the closing bracket.
            if (pRunningStr[0] == '}') {
                pContext->conversions.push_back(currentFunc);
                currentFunc.formatInStr = NULL;
                currentFunc.formatOutStr = NULL;
                currentFunc.instructions.clear();
                pRunningStr += 1;
                level -= 1;
                continue;
            }

            currentInst.name = pRunningStr;

            // Get to the end of the name and null terminate it.
            for (;;) {
                if (pRunningStr[0] == '\0') return -1;
                if (dr_is_whitespace(pRunningStr[0])) {
                    break;
                }
                pRunningStr += 1;
            }

            pRunningStr[0] = '\0';
            level += 1;
        } else if (level == 2) {
            // Inside an instruction.

            // Find the first non-whitespace which is where the next parameter should be located.
            for (;;) {
                if (pRunningStr[0] == '\0') return -1;
                if (!dr_is_whitespace(pRunningStr[0])) {
                    break;
                }
                pRunningStr += 1;
            }

            currentInst.params[currentParam++] = pRunningStr;
            currentInst.paramCount += 1;

            // Should be sitting at the next parameter or the semi-colon.
            if (pRunningStr[0] == ';') {
                currentFunc.instructions.push_back(currentInst);
                memset(&currentInst, 0, sizeof(currentInst));
                currentParam = 0;
                pRunningStr[0] = '\0';
                pRunningStr += 1;
                level -= 1;
                continue;
            }

            // Get to the end of the name and null terminate it.
            for (;;) {
                if (pRunningStr[0] == '\0') return -1;
                if (dr_is_whitespace(pRunningStr[0])) {
                    break;
                }

                if (pRunningStr[0] == ';') {
                    currentFunc.instructions.push_back(currentInst);
                    memset(&currentInst, 0, sizeof(currentInst));
                    currentParam = 0;
                    pRunningStr[0] = '\0';
                    pRunningStr += 1;
                    level -= 1;
                    break;
                }

                pRunningStr += 1;
            }

            pRunningStr[0] = '\0';
        }

        pRunningStr += 1;
    }

    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    malgen_context context;
    int result = malgen_compile(&context);
    if (result != 0) {
        return result;
    }

    FILE* pOutputFile = dr_fopen("malgen_test0.c", "w");
    if (pOutputFile == NULL) {
        printf("Failed to open output file.\n");
        return -2;
    }


    // We need conversion routines for each different combination of formats.



    // TESTING
    for (size_t i = 0; i < context.conversions.size(); ++i) {
        printf("%s to %s\n", context.conversions[i].formatInStr, context.conversions[i].formatOutStr);
        for (size_t j = 0; j < context.conversions[i].instructions.size(); ++j) {
            printf("   %s", context.conversions[i].instructions[j].name);
            for (int k = 0; k < context.conversions[i].instructions[j].paramCount; ++k) {
                printf(" %s", context.conversions[i].instructions[j].params[k]);
            }
            printf("\n");
        }
    }

    return 0;
}