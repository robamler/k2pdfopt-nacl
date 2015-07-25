/**
 * k2pdfopt_nacl.c
 * 
 * This file contains a stripped-down version of k2pdfopt's main()
 * function that is called by the Native Client module once it
 * received the command line parameters from the containing web
 * page.
 *
 * This file is based on the file "k2pdfopt.c", which is
 * Copyright (C) 2015  http://willus.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <k2pdfopt.h>

int k2pdfoptmain(int argc,char *argv[]) {
    int i;
    static K2PDFOPT_CONVERSION _k2conv,*k2conv;
    K2PDFOPT_SETTINGS *k2settings;
    static STRBUF _cmdline,_env,_usermenu;
    STRBUF *cmdline,*env,*usermenu;
    char funcname[13];
    strcpy (funcname, "k2pdfoptmain");
    k2conv=&_k2conv;
    k2pdfopt_conversion_init(k2conv);
    k2settings=&k2conv->k2settings;
    cmdline=&_cmdline;
    env=&_env;
    usermenu=&_usermenu;
    strbuf_init(cmdline);
    strbuf_init(env);  // Leave empty
    strbuf_init(usermenu);

    for (i=1;i<argc;i++)
        strbuf_cat_with_quotes(cmdline,argv[i]);
    k2sys_init();
    k2pdfopt_settings_init(k2settings);
    k2pdfopt_files_clear(&k2conv->k2files);

    parse_cmd_args(k2conv,env,cmdline,usermenu,1,0);
    k2settings->query_user = 0;
    k2settings->exit_on_complete = 1;
    ansi_set(0);  // turn off text coloring on STDOUT

    k2sys_header(NULL);

    /*
    ** Process files
    */
    for (i=0;i<k2conv->k2files.n;i++)
        {
        K2PDFOPT_OUTPUT k2out;

        k2out.outname=NULL;
        k2out.filecount=0;
        k2out.bmp=NULL;
        k2pdfopt_proc_wildarg(k2settings,k2conv->k2files.file[i],1,&k2out);
        willus_mem_free((double **)&k2out.outname,funcname);
        }

    /*
    ** All done.
    */
    k2sys_enter_to_exit(k2settings);
    k2sys_close(k2settings);
    strbuf_free(usermenu);
    strbuf_free(env);
    strbuf_free(cmdline);
    k2pdfopt_conversion_close(k2conv);
    return(0);
}
