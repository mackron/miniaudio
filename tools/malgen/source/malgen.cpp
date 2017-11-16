
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
    const char* userNamespace;
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
    
    switch (((unsigned long long)pIn & 0x15) / sizeof(float))
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


std::string malgen_get_format_c_type_string(const char* formatStr)
{
    if (strcmp(formatStr, "u8") == 0) {
        return "unsigned char";
    }
    if (strcmp(formatStr, "s16") == 0) {
        return "short";
    }
    if (strcmp(formatStr, "s24") == 0) {
        return "void";
    }
    if (strcmp(formatStr, "s32") == 0) {
        return "int";
    }
    if (strcmp(formatStr, "f32") == 0) {
        return "float";
    }

    return "";
}

std::string malgen_get_format_impl_c_type_string(const char* formatStr)
{
    if (strcmp(formatStr, "f32") == 0) {
        return "float";
    }

    return "int";
}

std::string malgen_generate_code__conversion_func_params(malgen_context* pContext, malgen_conversion_desc* pFuncDesc)
{
    std::string code;
    code += malgen_get_format_c_type_string(pFuncDesc->formatOutStr); code += "* pOut, ";
    code += "const "; code += malgen_get_format_c_type_string(pFuncDesc->formatInStr); code += "* pIn, ";
    code += "unsigned int count";

    return code;
}

std::string malgen_get_format_input_conversion_code(const char* formatStr)
{
    if (strcmp(formatStr, "s24") == 0) {
        return "((int)(((unsigned int)(((unsigned char*)pIn)[i*3+0]) << 8) | ((unsigned int)(((unsigned char*)pIn)[i*3+1]) << 16) | ((unsigned int)(((unsigned char*)pIn)[i*3+2])) << 24)) >> 8";
    }

    return "pIn[i]";
}

std::string malgen_get_format_output_conversion_code(const char* formatStr)
{
    if (strcmp(formatStr, "s24") == 0) {
        return "((unsigned char*)pOut)[(i*3)+0] = (unsigned char)(r & 0xFF); ((unsigned char*)pOut)[(i*3)+1] = (unsigned char)((r & 0xFF00) >> 8); ((unsigned char*)pOut)[(i*3)+2] = (unsigned char)((r & 0xFF0000) >> 16)";
    }

    std::string code;
    code = "pOut[i] = ("; code += malgen_get_format_c_type_string(formatStr); code += ")r";

    return code;
}

std::string malgen_format_op_param(const char* param)
{
    std::string s(param);

    // (flt) -> (float)
    {
        const char* src = "(flt)"; const char* dst = "(float)";
        size_t loc = s.find(src);
        if (loc != std::string::npos) {
            s = s.replace(loc, strlen(src), dst);
        }
    }

    // (dbl) -> (double)
    {
        const char* src = "(dbl)"; const char* dst = "(double)";
        size_t loc = s.find(src);
        if (loc != std::string::npos) {
            s = s.replace(loc, strlen(src), dst);
        }
    }

    // (uint) -> (unsigned int)
    {
        const char* src = "(uint)"; const char* dst = "(unsigned int)";
        size_t loc = s.find(src);
        if (loc != std::string::npos) {
            s = s.replace(loc, strlen(src), dst);
        }
    }

    // (lng) -> (mal_int64)
    {
        const char* src = "(lng)"; const char* dst = "(mal_int64)";
        size_t loc = s.find(src);
        if (loc != std::string::npos) {
            s = s.replace(loc, strlen(src), dst);
        }
    }

    return s;
}

std::string malgen_generate_code__conversion_func_inst_binary_op(const char* result, const char* param1, const char* param2, const char* op)
{
    bool requiresCast = false;
    const char* resultVar = result;
    if (resultVar[0] == '(') {
        for (;;) {
            if (resultVar[0] == '\0' || resultVar[0] == ')') {
                resultVar += 1;
                break;
            }
            resultVar += 1;
        }

        requiresCast = true;
    }

    std::string assignmentStr = malgen_format_op_param(param1) + " " + op + " " + malgen_format_op_param(param2);
    
    std::string code;
    code += resultVar; code += " = ";
    
    if (requiresCast) {
        char typeStr[64];
        strncpy_s(typeStr, result, resultVar - result);
        
        code += malgen_format_op_param(typeStr); code += "("; code += assignmentStr; code += ")";
        return code;
    } else {
        code += assignmentStr;
        return code;
    }
}

std::string malgen_generate_code__conversion_func_inst(malgen_context* pContext, malgen_instruction* pInst)
{
    std::string code;
    if (strcmp(pInst->name, "int") == 0) {
        code += "int "; code += pInst->params[0];
    }
    if (strcmp(pInst->name, "lng") == 0) {
        code += "mal_int64 "; code += pInst->params[0];
    }
    if (strcmp(pInst->name, "flt") == 0) {
        code += "float "; code += pInst->params[0];
    }
    if (strcmp(pInst->name, "dbl") == 0) {
        code += "double "; code += pInst->params[0];
    }

    if (strcmp(pInst->name, "add") == 0) {
        code += malgen_generate_code__conversion_func_inst_binary_op(pInst->params[0], pInst->params[1], pInst->params[2], "+");
    }
    if (strcmp(pInst->name, "sub") == 0) {
        code += malgen_generate_code__conversion_func_inst_binary_op(pInst->params[0], pInst->params[1], pInst->params[2], "-");
    }
    if (strcmp(pInst->name, "mul") == 0) {
        code += malgen_generate_code__conversion_func_inst_binary_op(pInst->params[0], pInst->params[1], pInst->params[2], "*");
    }
    if (strcmp(pInst->name, "div") == 0) {
        code += malgen_generate_code__conversion_func_inst_binary_op(pInst->params[0], pInst->params[1], pInst->params[2], "/");
    }

    if (strcmp(pInst->name, "shl") == 0) {
        code += malgen_generate_code__conversion_func_inst_binary_op(pInst->params[0], pInst->params[1], pInst->params[2], "<<");
    }
    if (strcmp(pInst->name, "shr") == 0) {
        code += malgen_generate_code__conversion_func_inst_binary_op(pInst->params[0], pInst->params[1], pInst->params[2], ">>");
    }

    if (strcmp(pInst->name, "mov") == 0) {
        code += pInst->params[0]; code += " = "; code += pInst->params[1];
    }

    if (strcmp(pInst->name, "sig") == 0) {  // <-- This gets the sign of the first input parameter and moves it to the result.
        code += pInst->params[0]; code += " = "; code += "(("; code += pInst->params[1]; code += " < 0) ? 1 : 0)"; // ((a < 0) ? 1 : 0)
    }

    if (strcmp(pInst->name, "clip") == 0) {  // clamp(a, -1, 1) -> r = ((a < -1) ? -1 : ((a > 1) ? 1 : a))
        code += pInst->params[0]; code += " = "; code += "(("; code += pInst->params[1]; code += " < -1) ? -1 : (("; code += pInst->params[1]; code += " > 1) ? 1 : "; code += pInst->params[1]; code += "))";
    }

    return code;
}

std::string malgen_generate_code__conversion_func_decl(malgen_context* pContext, malgen_conversion_desc* pFuncDesc)
{
    std::string code = "void ";
    code += pContext->userNamespace;
    code += pFuncDesc->formatInStr;
    code += "_to_";
    code += pFuncDesc->formatOutStr;
    code += "(";
    code += malgen_generate_code__conversion_func_params(pContext, pFuncDesc);
    code += ")";

    return code;
}

std::string malgen_generate_code__conversion_func_impl(malgen_context* pContext, malgen_conversion_desc* pFuncDesc)
{
    std::string code;

    code += "    "; code += malgen_get_format_impl_c_type_string(pFuncDesc->formatOutStr); code += " r;\n";
    code += "    for (unsigned int i = 0; i < count; ++i) {\n";
    code += "        "; code += malgen_get_format_impl_c_type_string(pFuncDesc->formatInStr); code += " x = "; code += malgen_get_format_input_conversion_code(pFuncDesc->formatInStr); code += ";\n";
    for (size_t i = 0; i < pFuncDesc->instructions.size(); ++i) {
        code += "        "; code += malgen_generate_code__conversion_func_inst(pContext, &pFuncDesc->instructions[i]); code += ";\n";
    }
    code += "        "; code += malgen_get_format_output_conversion_code(pFuncDesc->formatOutStr); code += ";\n";
    code += "    }";

    return code;
}

std::string malgen_generate_code__conversion_func(malgen_context* pContext, malgen_conversion_desc* pFuncDesc)
{
    std::string code = malgen_generate_code__conversion_func_decl(pContext, pFuncDesc);
    code += "\n{\n";
    code += malgen_generate_code__conversion_func_impl(pContext, pFuncDesc);
    code += "\n}\n";

    return code;
}

int malgen_generate_code(malgen_context* pContext, std::string* pCodeOut)
{
    std::string code;

    // Conversion functions.
    // Declarations.
    for (size_t i = 0; i < pContext->conversions.size(); ++i) {
        code += malgen_generate_code__conversion_func_decl(pContext, &pContext->conversions[i]);
        code += ";\n";
    }
    code += "\n";

    // Definitions.
    for (size_t i = 0; i < pContext->conversions.size(); ++i) {
        code += malgen_generate_code__conversion_func(pContext, &pContext->conversions[i]);
        code += "\n";
    }

    *pCodeOut = code;
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

    context.userNamespace = "mal_pcm_"; // TODO: Implement the "--namespace" command line argument.

    FILE* pOutputFile = dr_fopen("malgen_test0.c", "w");
    if (pOutputFile == NULL) {
        printf("Failed to open output file.\n");
        return -2;
    }

    std::string code;
    result = malgen_generate_code(&context, &code);
    if (result != 0) {
        printf("Code generation failed.\n");
        return result;
    }

    fprintf(pOutputFile, "%s", code.c_str());


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