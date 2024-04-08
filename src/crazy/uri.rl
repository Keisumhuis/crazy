#include "uri.h"
#include <sstream>

namespace crazy {
%%{
    # See RFC 3986: http://www.ietf.org/rfc/rfc3986.txt

    machine uri_parser;

    gen_delims = ":" | "/" | "?" | "#" | "[" | "]" | "@";
    sub_delims = "!" | "$" | "&" | "'" | "(" | ")" | "*" | "+" | "," | ";" | "=";
    reserved = gen_delims | sub_delims;
    unreserved = alpha | digit | "-" | "." | "_" | "~";
    pct_encoded = "%" xdigit xdigit;

    action marku { mark = fpc; }
    action markh { mark = fpc; }

    action save_scheme {
        uri->SetScheme(std::string(mark, fpc - mark));
        mark = NULL;
    }

    scheme = (alpha (alpha | digit | "+" | "-" | ".")*) >marku %save_scheme;

    action save_port {
        if (fpc != mark) {
            uri->SetPort(atoi(mark));
        }
        mark = NULL;
    }
    action save_userinfo {
        if(mark) {
            uri->SetUserInfo(std::string(mark, fpc - mark));
        }
        mark = NULL;
    }
    action save_host {
        if (mark != NULL) {
            uri->SetHost(std::string(mark, fpc - mark));
        }
    }

    userinfo = (unreserved | pct_encoded | sub_delims | ":")*;
    dec_octet = digit | [1-9] digit | "1" digit{2} | 2 [0-4] digit | "25" [0-5];
    IPv4address = dec_octet "." dec_octet "." dec_octet "." dec_octet;
    h16 = xdigit{1,4};
    ls32 = (h16 ":" h16) | IPv4address;
    IPv6address = (                         (h16 ":"){6} ls32) |
                  (                    "::" (h16 ":"){5} ls32) |
                  ((             h16)? "::" (h16 ":"){4} ls32) |
                  (((h16 ":"){1} h16)? "::" (h16 ":"){3} ls32) |
                  (((h16 ":"){2} h16)? "::" (h16 ":"){2} ls32) |
                  (((h16 ":"){3} h16)? "::" (h16 ":"){1} ls32) |
                  (((h16 ":"){4} h16)? "::"              ls32) |
                  (((h16 ":"){5} h16)? "::"              h16 ) |
                  (((h16 ":"){6} h16)? "::"                  );
    IPvFuture = "v" xdigit+ "." (unreserved | sub_delims | ":")+;
    IP_literal = "[" (IPv6address | IPvFuture) "]";
    reg_name = (unreserved | pct_encoded | sub_delims)*;
    host = IP_literal | IPv4address | reg_name;
    port = digit*;

    authority = ( (userinfo %save_userinfo "@")? host >markh %save_host (":" port >markh %save_port)? ) >markh;

    action save_segment {
        mark = NULL;
    }

    action save_path {
        uri->SetPath(std::string(mark, fpc - mark));
        mark = NULL;
    }


# pchar = unreserved | pct_encoded | sub_delims | ":" | "@";
# add (any -- ascii) support chinese
    pchar         = ( (any -- ascii ) | unreserved | pct_encoded | sub_delims | ":" | "@" ) ;
    segment = pchar*;
    segment_nz = pchar+;
    segment_nz_nc = (pchar - ":")+;

    action clear_segments {
    }

    path_abempty = (("/" segment))? ("/" segment)*;
    path_absolute = ("/" (segment_nz ("/" segment)*)?);
    path_noscheme = segment_nz_nc ("/" segment)*;
    path_rootless = segment_nz ("/" segment)*;
    path_empty = "";
    path = (path_abempty | path_absolute | path_noscheme | path_rootless | path_empty);

    action save_query {
        uri->SetQuery(std::string(mark, fpc - mark));
        mark = NULL;
    }
    action save_fragment {
        uri->SetFragment(std::string(mark, fpc - mark));
        mark = NULL;
    }

    query = (pchar | "/" | "?")* >marku %save_query;
    fragment = (pchar | "/" | "?")* >marku %save_fragment;

    hier_part = ("//" authority path_abempty > markh %save_path) | path_absolute | path_rootless | path_empty;

    relative_part = ("//" authority path_abempty) | path_absolute | path_noscheme | path_empty;
    relative_ref = relative_part ( "?" query )? ( "#" fragment )?;

    absolute_URI = scheme ":" hier_part ( "?" query )? ;
    # Obsolete, but referenced from HTTP, so we translate
    relative_URI = relative_part ( "?" query )?;

    URI = scheme ":" hier_part ( "?" query )? ( "#" fragment )?;
    URI_reference = URI | relative_ref;
    main := URI_reference;
    write data;
}%%

Uri::Ptr Uri::Create(const std::string& uristr) {
	Uri::Ptr uri(new Uri);
    	int cs = 0;
    	const char* mark = 0;
    	%% write init;
    	const char *p = uristr.c_str();
    	const char *pe = p + uristr.size();
    	const char* eof = pe;
    	%% write exec;
    	if(cs == uri_parser_error) {
        	return nullptr;
    	} else if(cs >= uri_parser_first_final) {
        	return uri;
    	}
    	return nullptr;
}
const std::string& Uri::GetScheme() const { return m_scheme; }
const std::string& Uri::GetUserInfo() const { return m_userinfo; }
const std::string& Uri::GetHost() const { return m_host; }
const std::string& Uri::GetPath() const { return m_path; }
const std::string& Uri::GetQuery() const { return m_query; }
const std::string& Uri::GetFragment() const { return m_fragment; }
const int32_t Uri::GetPort() const { 
	if (m_port) {
		return m_port;	
	}
	if (m_scheme == "http" || m_scheme == "ws") {
		return 80;
	} else if (m_scheme == "https" || m_scheme == "wss") {
		return 443;
	}
	return m_port;
}
void Uri::SetScheme(const std::string& val) { m_scheme = val; }
void Uri::SetUserInfo(const std::string& val) { m_userinfo = val; }
void Uri::SetHost(const std::string& val) { m_host = val; }
void Uri::SetPath(const std::string& val) { m_path = val; }
void Uri::SetQuery(const std::string& val) { m_query = val; }
void Uri::SetFragment(const std::string& val) { m_fragment = val; }
void Uri::SetPort(const int32_t val) { m_port = val; }
}
