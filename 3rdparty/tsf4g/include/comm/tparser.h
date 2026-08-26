#ifndef TPARSER_H
#define TPARSER_H

#include "rapidxml_c_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

unsigned int tree_save_file(txml_tree* pstTree, const char* pszFile);

void tparser_convert_string(unsigned char* pszStr);
void tparser_convert_element(txml_node* pstElement);

txml_parser* tparser_create(void);
#define tparser_free		txml_parser_free

#define tparser_load_file	txml_parser_load_file
#define tparser_load_fp		txml_parser_load_file_fp
#define tparser_load_buffer	txml_parser_load_buffer

txml_tree* tparser_tree(txml_parser* pstParser);



#ifdef __cplusplus
}
#endif

#endif /* TPARSER_H */
