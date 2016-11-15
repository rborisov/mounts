#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <string.h>
#include <iconv.h>
#include "id3v2lib.h"

void utf16tochar(uint16_t* string, int size)
{
    char buf[128];
    char *_src = (char*)string;
    size_t dstlen = 128;
    size_t srclen = size;
    char *_dst = buf;

    memset(buf, 0, 128);
//    iconv_t conv = iconv_open("UTF-8", "UTF-16");
    iconv_t conv = iconv_open("", "");
    iconv(conv, &_src, &srclen, &_dst, &dstlen);
    if (strlen(buf) == 0) {
        iconv_close(conv);
        conv = iconv_open("UTF-8", "UTF-16");
        _src = string;
        _dst = buf;
        dstlen = 128;
        iconv(conv, &_src, &size, &_dst, &dstlen);
    }
    printf("%d %d %s\n", dstlen, strlen(buf), buf);
    iconv_close(conv);

}

int main(int argc, char* argv[])
{
    printf("%s\n", argv[1]);
    ID3v2_tag* tag = load_tag(argv[1]); // Load the full tag from the file
    if(tag == NULL)
    {
        printf("new tag\n");
        tag = new_tag();
    }
//    setlocale(LC_ALL, "");


    // Load the fields from the tag
    ID3v2_frame* artist_frame = tag_get_artist(tag); // Get the full artist frame
    // We need to parse the frame content to make readable
    ID3v2_frame_text_content* artist_content = parse_text_frame_content(artist_frame);

    printf("ARTIST: %d %s\n", artist_content->size, artist_content->data); // Show the artist info
//    println_utf16(artist_content->data, artist_content->size);
    utf16tochar(artist_content->data, artist_content->size);

    ID3v2_frame* title_frame = tag_get_title(tag);
    ID3v2_frame_text_content* title_content = parse_text_frame_content(title_frame);
    printf("TITLE: %d %s\n", title_content->size, title_content->data);
//    println_utf16(title_content->data, title_content->size);
    utf16tochar(title_content->data, title_content->size);
    return 1;
}
