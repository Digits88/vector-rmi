/*
 * libwbxml, the WBXML Library.
 * Copyright (C) 2002-2005 Aymerick Jehanne <aymerick@jehanne.org>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * LGPL v2.1: http://www.gnu.org/copyleft/lesser.txt
 * 
 * Contact: libwbxml@aymerick.com
 * Home: http://libwbxml.aymerick.com
 */
 
/**
 * @file wbxml_tree_clb_xml.c
 * @ingroup wbxml_tree
 *
 * @author Aymerick Jehanne <libwbxml@aymerick.com>
 * @date 03/03/11
 *
 * @brief WBXML Tree Callbacks for XML Parser (Expat)
 */

#if defined( HAVE_EXPAT )

#include "wbxml.h"


/************************************
 *  Public Functions
 */

void wbxml_tree_clb_xml_decl(void           *ctx,
                             const XML_Char *version,
                             const XML_Char *encoding,
                             int             standalone)
{
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;

    if (tree_ctx->expat_utf16) {
        /** @todo Convert from UTF-16 to UTF-8 */
    }

    /* This handler is called for XML declarations and also for text declarations discovered 
     * in external entities. The way to distinguish is that the version parameter will
     * be NULL for text declarations.
     */
    if (version != NULL) {
        if (encoding != NULL) {
            /* Get encoding */
            if (!wbxml_charset_get_mib((const WB_TINY*)encoding, &(tree_ctx->tree->orig_charset))) {
                WBXML_WARNING((WBXML_CONV, "Charset Encoding not supported: %s", encoding));
            }
        }
    }
}


void wbxml_tree_clb_xml_doctype_decl(void           *ctx, 
                                     const XML_Char *doctypeName, 
                                     const XML_Char *sysid,
                                     const XML_Char *pubid, 
                                     int             has_internal_subset)
{    
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;
    const WBXMLLangEntry *lang_table = NULL;

    if (tree_ctx->expat_utf16) {
        /** @todo Convert from UTF-16 to UTF-8 */
    }

    /* Search for Language Table, given the XML Public ID and System ID */
    lang_table = wbxml_tables_search_table(wbxml_tables_get_main(), 
                                           (const WB_UTINY *) pubid, 
                                           (const WB_UTINY *) sysid, 
                                           NULL);

    if (lang_table != NULL) {
        /* Ho Yeah ! We got it ! */
        tree_ctx->tree->lang = lang_table;
    }
    else {
        /* We will try to find the Language Table, given the Root Element */
        WBXML_WARNING((WBXML_CONV, "Language Table NOT found, given the XML Public ID and System ID"));
    }
}


void wbxml_tree_clb_xml_start_element(void           *ctx,
                                      const XML_Char *localName,
                                      const XML_Char **attrs)
{
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;
    const WBXMLLangEntry *lang_table = NULL;

    if (tree_ctx->expat_utf16) {
        /** @todo Convert from UTF-16 to UTF-8 */
    }

    /* Check for Error */
    if (tree_ctx->error != WBXML_OK)
        return;

    /* Are we skipping a whole node ? */
    if (tree_ctx->skip_lvl > 0) {
        tree_ctx->skip_lvl++;
        return;
    }

    if (tree_ctx->current == NULL) {
        /* This is the Root Element */
        if (tree_ctx->tree->lang == NULL) {
            /* Language Table not already found: Search again */
            lang_table = wbxml_tables_search_table(wbxml_tables_get_main(), 
                                                   NULL, 
                                                   NULL, 
                                                   (const WB_UTINY *) localName);
        
            if (lang_table == NULL) {
                /* Damn, this is an unknown language for us... */
                tree_ctx->error = WBXML_ERROR_UNKNOWN_XML_LANGUAGE;
                return;
            }
            else {
                /* Well, we hope this was the Language we are searching for.. let's try with it :| */
                tree_ctx->tree->lang = lang_table;
            }
        }
    }

#if defined( WBXML_SUPPORT_SYNCML )

    /* If this is an embedded (not root) "DevInf" document, skip it */
    if ((WBXML_STRCMP(localName, "DevInf") == 0) &&
        (tree_ctx->current != NULL))
    {
        tree_ctx->skip_start = XML_GetCurrentByteIndex(tree_ctx->xml_parser);

        /* Skip this node */
        tree_ctx->skip_lvl++;

        return;
    }

#endif /* WBXML_SUPPORT_SYNCML */

    /* Add Element Node */
    tree_ctx->current = wbxml_tree_add_xml_elt_with_attrs(tree_ctx->tree,
                                                          tree_ctx->current,
                                                          (WB_UTINY *) localName,
                                                          (const WB_UTINY**) attrs);

    if (tree_ctx->current == NULL) {
        tree_ctx->error = WBXML_ERROR_NOT_ENOUGH_MEMORY;
    }
}


void wbxml_tree_clb_xml_end_element(void           *ctx,
                                    const XML_Char *localName)
{
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;
#if defined( WBXML_SUPPORT_SYNCML )
    WBXMLBuffer *devinf_doc = NULL;
    WBXMLTree *tree = NULL;
    WBXMLError ret = WBXML_OK;
#endif /* WBXML_SUPPORT_SYNCML */

    if (tree_ctx->expat_utf16) {
        /** @todo Convert from UTF-16 to UTF-8 */
    }

    /* Check for Error */
    if (tree_ctx->error != WBXML_OK)
        return;

    /* Are we skipping a whole node ? */
    if (tree_ctx->skip_lvl > 0) {
        if (tree_ctx->skip_lvl == 1)
        {
            /* End of skipped node */

#if defined( WBXML_SUPPORT_SYNCML )
            if (WBXML_STRCMP(localName, "DevInf") == 0) {
                /* Get embedded DevInf Document */
                devinf_doc = wbxml_buffer_create(tree_ctx->input_buff + tree_ctx->skip_start, 
                                                 XML_GetCurrentByteIndex(tree_ctx->xml_parser) - tree_ctx->skip_start,
                                                 XML_GetCurrentByteIndex(tree_ctx->xml_parser) - tree_ctx->skip_start + 10);

                if (tree_ctx->expat_utf16) {
                    /** @todo Convert from UTF-16 to UTF-8 */
                }

                /* Check Buffer Creation and addd </DevInf> ending tag */
                if ((devinf_doc == NULL) || (!wbxml_buffer_append_cstr(devinf_doc, "</DevInf>")))
                {
                    tree_ctx->error = WBXML_ERROR_NOT_ENOUGH_MEMORY;
                    wbxml_buffer_destroy(devinf_doc);
                    return;
                }

                WBXML_DEBUG((WBXML_PARSER, "\t DevInf Doc : '%s'", wbxml_buffer_get_cstr(devinf_doc)));

                /* Parse 'DevInf' Document */
                if ((ret = wbxml_tree_from_xml(wbxml_buffer_get_cstr(devinf_doc),
                                               wbxml_buffer_len(devinf_doc),
                                               &tree)) != WBXML_OK)
                {
                    tree_ctx->error = ret;
                    wbxml_buffer_destroy(devinf_doc);
                    return;
                }

                /* Add Tree Node */
                tree_ctx->current = wbxml_tree_add_tree(tree_ctx->tree,
                                                        tree_ctx->current,
                                                        tree);
                if (tree_ctx->current == NULL)
                {
                    tree_ctx->error = WBXML_ERROR_INTERNAL;
                    wbxml_tree_destroy(tree);
                    wbxml_buffer_destroy(devinf_doc);
                    return;
                }

                /* Clean-up */
                wbxml_buffer_destroy(devinf_doc);
                tree_ctx->skip_lvl = 0;
            }
#endif /* WBXML_SUPPORT_SYNCML */
        }
        else {
            tree_ctx->skip_lvl--;
            return;
        }
    }

    if (tree_ctx->current == NULL) {
        tree_ctx->error = WBXML_ERROR_INTERNAL;
        return;
    }

    if (tree_ctx->current->parent == NULL) {
        /* This must be the Root Element */
        if (tree_ctx->current != tree_ctx->tree->root) {
            tree_ctx->error = WBXML_ERROR_INTERNAL;
        }
    }
    else {
#if defined ( WBXML_SUPPORT_SYNCML )
        /* Have we added a missing CDATA section ? 
         * If so, we assume that now that we have reached an end of Element, 
         * the CDATA section ended, and so we go back to parent.
         */
        if ((tree_ctx->current != NULL) && (tree_ctx->current->type == WBXML_TREE_CDATA_NODE))
            tree_ctx->current = tree_ctx->current->parent;
#endif /* WBXML_SUPPORT_SYNCML */

        /* Go back one step upper in the tree */
        tree_ctx->current = tree_ctx->current->parent;
    }
}


void wbxml_tree_clb_xml_start_cdata(void *ctx)
{
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;

    /* Check for Error */
    if (tree_ctx->error != WBXML_OK)
        return;

    /* Are we skipping a whole node ? */
    if (tree_ctx->skip_lvl > 0)
        return;

    /* Add CDATA Node */
    tree_ctx->current = wbxml_tree_add_cdata(tree_ctx->tree, tree_ctx->current);
    if (tree_ctx->current == NULL) {
        tree_ctx->error = WBXML_ERROR_INTERNAL;
    }
}


void wbxml_tree_clb_xml_end_cdata(void *ctx)
{
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;

    /* Check for Error */
    if (tree_ctx->error != WBXML_OK)
        return;

    /* Are we skipping a whole node ? */
    if (tree_ctx->skip_lvl > 0)
        return;
        
    if (tree_ctx->current == NULL) {
        tree_ctx->error = WBXML_ERROR_INTERNAL;
        return;
    }

    if (tree_ctx->current->parent == NULL) {
        /* This must be the Root Element */
        if (tree_ctx->current != tree_ctx->tree->root) {
            tree_ctx->error = WBXML_ERROR_INTERNAL;
        }
    }
    else {
        /* Go back one step upper in the tree */
        tree_ctx->current = tree_ctx->current->parent;
    }
}


void wbxml_tree_clb_xml_characters(void           *ctx,
                                   const XML_Char *ch,
                                   int             len)
{
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;

    if (tree_ctx->expat_utf16) {
        /** @todo Convert from UTF-16 to UTF-8 */
    }

    /* Check for Error */
    if (tree_ctx->error != WBXML_OK)
        return;

    /* Are we skipping a whole node ? */
    if (tree_ctx->skip_lvl > 0)
        return;

#if defined ( WBXML_SUPPORT_SYNCML )
    /* Specific treatment for SyncML */
    switch (wbxml_tree_node_get_syncml_data_type(tree_ctx->current)) {
    case WBXML_SYNCML_DATA_TYPE_CLEAR:
    case WBXML_SYNCML_DATA_TYPE_DIRECTORY_VCARD:
    case WBXML_SYNCML_DATA_TYPE_VCALENDAR:
    case WBXML_SYNCML_DATA_TYPE_VCARD:
    case WBXML_SYNCML_DATA_TYPE_VOBJECT:
        /*
         * Add a missing CDATA section node
         *
         * Example:
         * <Add>
         *   <CmdID>6</CmdID>
         *   <Meta><Type xmlns='syncml:metinf'>text/x-vcard</Type></Meta>
         *   <Item>
         *     <Source>
         *         <LocURI>pas-id-3F4B790300000000</LocURI>
         *     </Source>         
         *     <Data>BEGIN:VCARD
         *  VERSION:2.1
         *  X-EVOLUTION-FILE-AS:Ximian, Inc.
         *  N:
         *  LABEL;WORK;ENCODING=QUOTED-PRINTABLE:401 Park Drive  3 West=0ABoston, MA
         *  02215=0AUSA
         *  TEL;WORK;VOICE:(617) 236-0442
         *  TEL;WORK;FAX:(617) 236-8630
         *  EMAIL;INTERNET:[EMAIL PROTECTED]
         *  URL:www.ximian.com/
         *  ORG:Ximian, Inc.
         *  NOTE:Welcome to the Ximian Addressbook.
         *  UID:pas-id-3F4B790300000000
         *  END:VCARD</Data>
         *   </Item>
         * </Add>
         *
         * The end of CDATA section is assumed to be reached when parsing the end 
         * of </Data> element.
         *
         * This kind of document is erroneous, but we must handle it.
         * Normally, this should be:
         *
         *  ...
         *     <Data><!CDATA[[BEGIN:VCARD
         *  VERSION:2.1
         *  X-EVOLUTION-FILE-AS:Ximian, Inc.
         *  ...
         *  UID:pas-id-3F4B790300000000
         *  END:VCARD
         *  ]]></Data>
         *  ...
         */

        /* 
         * We add a missing CDATA section if we are not already in a CDATA section.
         *
         * We don't add a CDATA section if we have already added a CDATA section. This
         * permits to correctly handle good XML documents like this:
         *
         *  ...
         *     <Data><!CDATA[[BEGIN:VCARD
         *  VERSION:2.1
         *  X-EVOLUTION-FILE-AS:Ximian, Inc.
         *  ...
         *  UID:pas-id-3F4B790300000000
         *  END:VCARD
         *  ]]>
         *     </Data>
         *  ...
         *
         * In this example, the spaces beetwen "]]>" and "</Data>" mustn't be added
         * to a CDATA section. 
         */
        if ((tree_ctx->current != NULL) && 
            (tree_ctx->current->type != WBXML_TREE_CDATA_NODE) &&
            !((tree_ctx->current->children != NULL) && 
              (tree_ctx->current->children->type == WBXML_TREE_CDATA_NODE)))
        {
            /* Add CDATA Node */
            tree_ctx->current = wbxml_tree_add_cdata(tree_ctx->tree, tree_ctx->current);
            if (tree_ctx->current == NULL) {
                tree_ctx->error = WBXML_ERROR_INTERNAL;
                return;
            }
        }

        /* Now we can add the Text Node */
        break;

    default:
        /* NOP */
        break;
    } /* switch */
#endif /* WBXML_SUPPORT_SYNCML */

    /* Add Text Node */
    if (wbxml_tree_add_text(tree_ctx->tree,
                            tree_ctx->current,
                            (const WB_UTINY*) ch,
                            len) == NULL)
    {
        tree_ctx->error = WBXML_ERROR_INTERNAL;
    }
}


void wbxml_tree_clb_xml_pi(void           *ctx,
                           const XML_Char *target,
                           const XML_Char *data)
{
    WBXMLTreeClbCtx *tree_ctx = (WBXMLTreeClbCtx *) ctx;

    if (tree_ctx->expat_utf16) {
        /** @todo Convert from UTF-16 to UTF-8 */
    }

    /* Check for Error */
    if (tree_ctx->error != WBXML_OK)
        return;

    /* Are we skipping a whole node ? */
    if (tree_ctx->skip_lvl > 0)
        return;

    /** @todo wbxml2xml_clb_pi() */
}

#endif /* HAVE_EXPAT */
