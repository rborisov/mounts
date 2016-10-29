#include <stdlib.h>
#include <extractor.h>
#include <getopt.h>
#include <signal.h>
#include <libintl.h>
#include <errno.h>
#include <string.h>

#define EXE

#define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#define _(a) dgettext("org.gnunet.libextractor", a)

static int in_process;

static int print_selected_keywords (void *cls,
        const char *plugin_name,
        enum EXTRACTOR_MetaType type,
        enum EXTRACTOR_MetaFormat format,
        const char *data_mime_type,
        const char *data,
        size_t data_len)
{
    char *keyword;
    const char *stype;
    const char *mt;

    mt = EXTRACTOR_metatype_to_string (type);
    stype = (NULL == mt) ? _("unknown") : gettext(mt);
    switch (format)
    {
        case EXTRACTOR_METAFORMAT_UNKNOWN:
            fprintf (stdout,
                    _("%s - (unknown, %u bytes)\n"),
                    stype,
                    (unsigned int) data_len);
            break;
        case EXTRACTOR_METAFORMAT_UTF8:
            keyword = strdup (data);
            if (NULL != keyword)
            {
                fprintf (stdout,
                        "%s %s\n",
                        stype,
                        keyword);
                free (keyword);
            }
            break;
        case EXTRACTOR_METAFORMAT_BINARY:
            fprintf (stdout,
                    _("%s - (binary, %u bytes)\n"),
                    stype,
                    (unsigned int) data_len);
            break;
        case EXTRACTOR_METAFORMAT_C_STRING:
            fprintf (stdout,
                    "%s - %.*s\n",
                    stype,
                    (int) data_len,
                    data);
            break;
        default:
            break;
    }
    return 0;
}



#ifdef EXE
int main(int argc, char *argv[])
{
    struct EXTRACTOR_PluginList *plugins = NULL;
    EXTRACTOR_MetaDataProcessor processor = NULL;
    /*
    plugins = EXTRACTOR_plugin_add_defaults (in_process
            ? EXTRACTOR_OPTION_IN_PROCESS
            : EXTRACTOR_OPTION_DEFAULT_POLICY);
*/
    processor = &print_selected_keywords;

    EXTRACTOR_extract (plugins,
            argv[1],
            NULL, 0,
            processor,
            NULL);

    EXTRACTOR_plugin_remove_all (plugins);

    return 0;
}
#endif
