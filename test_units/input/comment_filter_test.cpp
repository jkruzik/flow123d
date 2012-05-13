/*
 * comment_filter_test.cpp
 *
 *  Created on: May 7, 2012
 *      Author: jb
 */

#include <gtest/gtest.h>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include "input/comment_filter.hh"

using namespace std;

const string flow_json_comment_parser = R"JSON(
# komentar na zacatku, obsahuje humus { " } " : # \ \\ \ \{ \" \} \: \# 
{
# komentar uvnitr, obsahuje humus { " } " : # \ \\ \ \{ \" \} \: \#


    "text0"           : "text",

#viceradkovy dlouhy komentar \
pokracovani komentare \
jeste dalsi pokracovani komentare \
pokracovani s humusem { " } " : # \ \\ \ \{ \" \} \: \# \
pokracovani bez humusu    
    
    "text1"           : "text", # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
     
    "text2"           : "text" # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    , 
    
    "text3"           : # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    "text",
     
    "text4"          # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
     : "text",

    "text5\""           : "text",
    "text6\"\""           : "text",
    "text7"           : "text\"",
    "text8"           : "text\"\"",
    "text9\""           : "text\"",
    "text10#"           : "text",
    "text11"           : "text#",
    "text12#"           : "text#",

    "record0" : { # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
        "subrecord0"  : 1, # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \# 
        
        "subrecord1"  : 1 # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
        ,
         
        "subrecord2"  : # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \# 
        1,
         
        "subrecord3"  # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
        : 1
         
    }, # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    
    "record1" : { } # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    ,
    
    "record2" : # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \# 
    { },
    
    "record3" # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \# 
    : { },


    "z_array0" : [ [0], { "a" : 1 }, 2, {}, [] ], # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    "z_array1" : [ [0], { "a" : 1 }, 2, {}, [] 
    ],
    
    "z_array2" : [ [0], { "a" : 1 }, 2, {}, [ # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    ] ],
    
    "z_array3" : [ [0], { "a" : 1 }, 2, {}, # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
     [] ],
     
    "z_array4" : [ [0], { "a" : 1 }, 2, { # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    }, [] ],
    
    "z_array5" : [ [0], { "a" : # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
     1 }, 2, {}, [] ],
     
    "z_array6" : [ 
    [0], { "a" : 1 }, 2, {}, [] ], # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    
    "z_array7" :
     [ [0], { "a" : 1 }, 2, {}, [] ], # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
     
    "z_array8"
     : [ [0], { "a" : 1 }, 2, {}, [] ] # komentar s humusem { " } " : # \ \\ \ \{ \" \} \: \#
    
    key9 = "something"# This line contains spaces before end of line \   
here the comment should continue, but not after next line \

key10="something else" # previous line is empty and if also eaten by comment filer, the keys 9 ans 10 are not separated
}
# komentar na konci, obsahuje humus { " } " : # \ \\ \ \{ \" \} \: \#
)JSON";

string filter(const string& in) {
    namespace io = boost::iostreams;

    stringstream in_stream(in);
    ostringstream out_stream;
    io::filtering_istream fin;

    fin.push(Input::uncommenting_filter());
    fin.push(in_stream);

    char c;
    while (fin.get(c)) out_stream.put(c);

    return out_stream.str();
}

TEST(Storage, comment_filter) {


    // This seems to produce correct output, but to be sure we have small inputs compared to correct outputs
    cout << filter(flow_json_comment_parser);

    // check comments ending with Windows line ends
    EXPECT_EQ("\n\r\n\r", filter("# comment \n\r\n\r"));

}


