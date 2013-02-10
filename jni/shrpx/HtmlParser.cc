/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "HtmlParser.h"

#include <libxml/uri.h>

#include "util.h"

namespace spdylay {

ParserData::ParserData(const std::string& base_uri)
  : base_uri(base_uri)
{}

HtmlParser::HtmlParser(const std::string& base_uri)
  : base_uri_(base_uri),
    parser_ctx_(0),
    parser_data_(base_uri)
{}

HtmlParser::~HtmlParser()
{
  htmlFreeParserCtxt(parser_ctx_);
}

namespace {
const char* get_attr(const xmlChar **attrs, const char *name)
{
  for(; *attrs; attrs += 2) {
    if(util::strieq(reinterpret_cast<const char*>(attrs[0]), name)) {
      return reinterpret_cast<const char*>(attrs[1]);
    }
  }
  return 0;
}
} // namespace

namespace {
void start_element_func
(void* user_data,
 const xmlChar *name,
 const xmlChar **attrs)
{
  ParserData *parser_data = reinterpret_cast<ParserData*>(user_data);
  if(util::strieq(reinterpret_cast<const char*>(name), "link")) {
    const char *rel_attr = get_attr(attrs, "rel");
    const char *href_attr = get_attr(attrs, "href");
    if((util::strieq(rel_attr, "shortcut icon") ||
        util::strieq(rel_attr, "stylesheet")) &&
       href_attr) {
      xmlChar *u = xmlBuildURI(reinterpret_cast<const xmlChar*>(href_attr),
                               reinterpret_cast<const xmlChar*>
                               (parser_data->base_uri.c_str()));
      if(u) {
        parser_data->links.push_back(reinterpret_cast<char*>(u));
        free(u);
      }
    }
  } else if(util::strieq(reinterpret_cast<const char*>(name), "img")) {
    const char *src_attr = get_attr(attrs, "src");
    if(src_attr) {
      xmlChar *u = xmlBuildURI(reinterpret_cast<const xmlChar*>(src_attr),
                               reinterpret_cast<const xmlChar*>
                               (parser_data->base_uri.c_str()));
      if(u) {
        parser_data->links.push_back(reinterpret_cast<char*>(u));
        free(u);
      }
    }
  }
}
} // namespace

namespace {
xmlSAXHandler saxHandler =
  {
    0, // internalSubsetSAXFunc
    0, // isStandaloneSAXFunc
    0, // hasInternalSubsetSAXFunc
    0, // hasExternalSubsetSAXFunc
    0, // resolveEntitySAXFunc
    0, // getEntitySAXFunc
    0, // entityDeclSAXFunc
    0, // notationDeclSAXFunc
    0, // attributeDeclSAXFunc
    0, // elementDeclSAXFunc
    0, //   unparsedEntityDeclSAXFunc
    0, //   setDocumentLocatorSAXFunc
    0, //   startDocumentSAXFunc
    0, //   endDocumentSAXFunc
    &start_element_func, //   startElementSAXFunc
    0, //   endElementSAXFunc
    0, //   referenceSAXFunc
    0, //   charactersSAXFunc
    0, //   ignorableWhitespaceSAXFunc
    0, //   processingInstructionSAXFunc
    0, //   commentSAXFunc
    0, //   warningSAXFunc
    0, //   errorSAXFunc
    0, //   fatalErrorSAXFunc
    0, //   getParameterEntitySAXFunc
    0, //   cdataBlockSAXFunc
    0, //   externalSubsetSAXFunc
    0, //   unsigned int        initialized
    0, //   void *      _private
    0, //   startElementNsSAX2Func
    0, //   endElementNsSAX2Func
    0, //   xmlStructuredErrorFunc
  };
} // namespace

int HtmlParser::parse_chunk(const char *chunk, size_t size, int fin)
{
  if(!parser_ctx_) {
    parser_ctx_ = htmlCreatePushParserCtxt(&saxHandler,
                                           &parser_data_,
                                           chunk, size,
                                           base_uri_.c_str(),
                                           XML_CHAR_ENCODING_NONE);
    if(!parser_ctx_) {
      return -1;
    } else {
      if(fin) {
        return parse_chunk_internal(0, 0, fin);
      } else {
        return 0;
      }
    }
  } else {
    return parse_chunk_internal(chunk, size, fin);
  }
}

int HtmlParser::parse_chunk_internal(const char *chunk, size_t size,
                                     int fin)
{
  int rv = htmlParseChunk(parser_ctx_, chunk, size, fin);
  if(rv == 0) {
    return 0;
  } else {
    return -1;
  }
}

const std::vector<std::string>& HtmlParser::get_links() const
{
  return parser_data_.links;
}

void HtmlParser::clear_links()
{
  parser_data_.links.clear();
}

} // namespace spdylay
