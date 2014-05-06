#include "xmlreader.h"

Rinfo parse_xml(const char *filename) {
  FILE *in = fopen(filename, "rb");
  if (!in) throw_error("xmlreader: can't open %s for reading", filename);

  Rinfo res;

  try {
    long len;
    char *buf;
    size_t result;
    // Go to end
    fseek(in,0,SEEK_END);
    // Get position at end (length)
    len = ftell(in);
    // Go to beginning
    fseek(in,0,SEEK_SET);
    // Malloc buffer
    buf = (char *)malloc(len);
    // Read into buffer
    result = fread(buf,len,1,in);
    // Check whether fread was successful
    if (result != 1) {
      fputs("XML Reading Error: ", stderr);
      exit(3);
    }
    fclose(in);

    rapidxml::xml_document<> doc;
    doc.parse<0>(buf);

    rapidxml::xml_node<> *node;

    if ((node = doc.first_node("QuadTreeInfo")) && (node = node->first_node("bounding_box")) && (node = node->first_node("bbox")) &&
      (node = node->first_node("min")) && (node = node->first_node("vector")) && (node = node->first_node("elt"))) {
      res.minx = strtod(node->value(), NULL);
      if ((node = node->next_sibling())) {
        res.miny = strtod(node->value(), NULL);
      }
    }
    if ((node = doc.first_node("QuadTreeInfo")) && (node = node->first_node("bounding_box")) && (node = node->first_node("bbox")) &&
      (node = node->first_node("max")) && (node = node->first_node("vector")) && (node = node->first_node("elt"))) {
      res.maxx = strtod(node->value(), NULL);
      if ((node = node->next_sibling())) {
        res.maxy = strtod(node->value(), NULL);
      }
    }
    if ((node = doc.first_node("QuadTreeInfo")) && (node = node->first_node("projection_size")) &&
      (node = node->first_node("vector")) && (node = node->first_node("elt"))) {
      res.projx = strtod(node->value(), NULL);
      if ((node = node->next_sibling())) {
        res.projy = strtod(node->value(), NULL);
      }
    }
  } catch (const std::exception &e) {
    throw_error("Parsing XML Error: %s", e.what());
  }
  return res;
}
