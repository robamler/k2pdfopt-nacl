#ifndef _K2PDFOPT_MODULE_H
#define _K2PDFOPT_MODULE_H

int pp_post_message(const char* category, const char* format, ...);
int pp_post_progress(int current_page, int page_count);

#endif /* _K2PDFOPT_MODULE_H */
