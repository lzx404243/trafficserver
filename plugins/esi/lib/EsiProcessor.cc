/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#include "EsiProcessor.h"
#include "Stats.h"
#include <ts/ts.h>
#include <cctype>

using std::string;
using namespace EsiLib;
// this needs to be a fixed address as only the address is used for comparison
const char *EsiProcessor::INCLUDE_DATA_ID_ATTR = reinterpret_cast<const char *>(0xbeadface);

#define FAILURE_INFO_TAG "plugin_esi_failureInfo"

namespace
{
DbgCtl dbg_ctl{"plugin_esi_procesor"};
}

// This can only be used in member functions of EsiProcessor.
//
#define DBG(FMT, ...) Dbg(dbg_ctl, FMT " contp=%p", ##__VA_ARGS__, _cont_addr)

EsiProcessor::EsiProcessor(void *cont_addr, HttpDataFetcher &fetcher, Variables &variables, const HandlerManager &handler_mgr)
  : _curr_state(STOPPED),
    _parser(),
    _n_prescanned_nodes(0),
    _n_processed_nodes(0),
    _n_processed_try_nodes(0),
    _overall_len(0),
    _fetcher(fetcher),
    _usePackedNodeList(false),
    _esi_vars(variables),
    _expression(variables),
    _n_try_blocks_processed(0),
    _handler_manager(handler_mgr),
    _cont_addr(cont_addr)
{
}

bool
EsiProcessor::start()
{
  if (_curr_state != STOPPED) {
    DBG("[%s] Implicit call to stop()", __FUNCTION__);
    stop();
  }
  _curr_state        = PARSING;
  _usePackedNodeList = false;
  return true;
}

bool
EsiProcessor::addParseData(const char *data, int data_len)
{
  if (_curr_state == ERRORED) {
    return false;
  }
  if (_curr_state == STOPPED) {
    DBG("[%s] Implicit call to start()", __FUNCTION__);
    start();
  } else if (_curr_state != PARSING) {
    DBG("[%s] Can only parse in parse stage", __FUNCTION__);
    return false;
  }

  if (!_parser.parseChunk(data, _node_list, data_len)) {
    TSError("[%s] Failed to parse chunk; Stopping processor...", __FUNCTION__);
    error();
    Stats::increment(Stats::N_PARSE_ERRS);
    return false;
  }
  if (!_preprocess(_node_list, _n_prescanned_nodes)) {
    TSError("[%s] Failed to preprocess parsed nodes; Stopping processor...", __FUNCTION__);
    error();
    return false;
  }
  return true;
}

bool
EsiProcessor::completeParse(const char *data /* = 0 */, int data_len /* = -1 */)
{
  if (_curr_state == ERRORED) {
    return false;
  }
  if (_curr_state == STOPPED) {
    DBG("[%s] Implicit call to start()", __FUNCTION__);
    start();
  } else if (_curr_state != PARSING) {
    DBG("[%s] Can only parse in parse stage", __FUNCTION__);
    return false;
  }

  if (!_parser.completeParse(_node_list, data, data_len)) {
    TSError("[%s] Couldn't parse ESI document", __FUNCTION__);
    error();
    Stats::increment(Stats::N_PARSE_ERRS);
    return false;
  }
  return _handleParseComplete();
}

EsiProcessor::UsePackedNodeResult
EsiProcessor::usePackedNodeList(const char *data, int data_len)
{
  if (_curr_state != STOPPED) {
    TSError("[%s] Cannot use packed node list whilst processing other data", __FUNCTION__);
    return PROCESS_IN_PROGRESS;
  }
  start();
  if (!_node_list.unpack(data, data_len)) {
    TSError("[%s] Could not unpack node list from provided data!", __FUNCTION__);
    error();
    return UNPACK_FAILURE;
  }
  _usePackedNodeList = true;
  return _handleParseComplete() ? PROCESS_SUCCESS : PROCESS_FAILURE;
}

bool
EsiProcessor::_handleParseComplete()
{
  if (_curr_state != PARSING) {
    DBG("[%s] Cannot handle parse complete in state %d", __FUNCTION__, _curr_state);
    return false;
  }
  if (!_preprocess(_node_list, _n_prescanned_nodes)) {
    TSError("[%s] Failed to preprocess parsed nodes; Stopping processor...", __FUNCTION__);
    error();
    return false;
  }
  for (IncludeHandlerMap::iterator map_iter = _include_handlers.begin(); map_iter != _include_handlers.end(); ++map_iter) {
    map_iter->second->handleParseComplete();
  }

  DBG("[%s] Parsed ESI document with %d nodes", __FUNCTION__, int(_node_list.size()));
  _curr_state = WAITING_TO_PROCESS;

  return true;
}

DataStatus
EsiProcessor::_getIncludeStatus(const DocNode &node)
{
  DBG("[%s] inside getIncludeStatus", __FUNCTION__);
  if (node.type == DocNode::TYPE_INCLUDE) {
    const Attribute &url = node.attr_list.front();

    if (url.value_len == 0) { // allow empty url
      return STATUS_DATA_AVAILABLE;
    }

    string raw_url(url.value, url.value_len);
    StringHash::iterator iter = _include_urls.find(raw_url);
    if (iter == _include_urls.end()) {
      TSError("[%s] Data not requested for URL [%.*s]; no data to include", __FUNCTION__, url.value_len, url.value);
      return STATUS_ERROR;
    }
    const string &processed_url = iter->second;
    DataStatus status           = _fetcher.getRequestStatus(processed_url);
    DBG("[%s] Got status %d successfully for URL [%.*s]", __FUNCTION__, status, int(processed_url.size()), processed_url.data());
    return status;
  } else if (node.type == DocNode::TYPE_SPECIAL_INCLUDE) {
    int include_data_id            = 0;
    SpecialIncludeHandler *handler = nullptr;
    for (AttributeList::const_iterator attr_iter = node.attr_list.begin(); attr_iter != node.attr_list.end(); ++attr_iter) {
      if (attr_iter->name == INCLUDE_DATA_ID_ATTR) {
        include_data_id = attr_iter->value_len;
        handler         = reinterpret_cast<SpecialIncludeHandler *>(const_cast<char *>(attr_iter->value));
        break;
      }
    }
    if (include_data_id == 0 || handler == nullptr) {
      TSError("[%s] Fail to find the special include data id attribute", __FUNCTION__);
      return STATUS_ERROR;
    }
    DataStatus status = handler->getIncludeStatus(include_data_id);
    DBG("[%s] Successfully got status %d for special include with id %d", __FUNCTION__, int(status), int(include_data_id));

    return status;
  }
  DBG("[%s] node of type %s", __FUNCTION__, DocNode::type_names_[node.type]);
  return STATUS_DATA_AVAILABLE;
}

bool
EsiProcessor::_getIncludeData(const DocNode &node, const char **content_ptr /* = 0 */, int *content_len_ptr /* = 0 */)
{
  if (node.type == DocNode::TYPE_INCLUDE) {
    const Attribute &url = node.attr_list.front();

    if (url.value_len == 0) { // allow empty url
      if (content_ptr && content_len_ptr) {
        *content_ptr     = nullptr;
        *content_len_ptr = 0;
        return true;
      } else {
        return false;
      }
    }

    string raw_url(url.value, url.value_len);
    StringHash::iterator iter = _include_urls.find(raw_url);
    if (iter == _include_urls.end()) {
      TSError("[%s] Data not requested for URL [%.*s]; no data to include", __FUNCTION__, url.value_len, url.value);
      return false;
    }
    const string &processed_url = iter->second;
    bool result;
    if (content_ptr && content_len_ptr) {
      result = _fetcher.getContent(processed_url, *content_ptr, *content_len_ptr);
    } else {
      result = (_fetcher.getRequestStatus(processed_url) == STATUS_DATA_AVAILABLE);
    }
    if (!result) {
      TSError("[%s] Couldn't get content for URL [%.*s]", __FUNCTION__, int(processed_url.size()), processed_url.data());
      Stats::increment(Stats::N_INCLUDE_ERRS);
      return false;
    }
    DBG("[%s] Got content successfully for URL [%.*s]", __FUNCTION__, int(processed_url.size()), processed_url.data());
    return true;
  } else if (node.type == DocNode::TYPE_SPECIAL_INCLUDE) {
    int include_data_id            = 0;
    SpecialIncludeHandler *handler = nullptr;
    for (AttributeList::const_iterator attr_iter = node.attr_list.begin(); attr_iter != node.attr_list.end(); ++attr_iter) {
      if (attr_iter->name == INCLUDE_DATA_ID_ATTR) {
        include_data_id = attr_iter->value_len;
        handler         = reinterpret_cast<SpecialIncludeHandler *>(const_cast<char *>(attr_iter->value));
        break;
      }
    }
    if (include_data_id == 0 || handler == nullptr) {
      TSError("[%s] Fail to find the special include data id attribute", __FUNCTION__);
      Stats::increment(Stats::N_SPCL_INCLUDE_ERRS);
      return false;
    }
    bool result;
    if (content_ptr && content_len_ptr) {
      result = handler->getData(include_data_id, *content_ptr, *content_len_ptr);
    } else {
      result = (handler->getIncludeStatus(include_data_id) == STATUS_DATA_AVAILABLE);
    }
    if (!result) {
      TSError("[%s] Couldn't get content for special include with id %d", __FUNCTION__, include_data_id);
      Stats::increment(Stats::N_SPCL_INCLUDE_ERRS);
      return false;
    }
    DBG("[%s] Successfully got content for special include with id %d", __FUNCTION__, include_data_id);
    return true;
  }
  TSError("[%s] Cannot get include data for node of type %s", __FUNCTION__, DocNode::type_names_[node.type]);
  return false;
}

EsiProcessor::ReturnCode
EsiProcessor::process(const char *&data, int &data_len)
{
  if (_curr_state == ERRORED) {
    return FAILURE;
  }
  if (_curr_state != WAITING_TO_PROCESS) {
    TSError("[%s] Processor has to finish parsing via completeParse() before process() call", __FUNCTION__);
    return FAILURE;
  }
  DocNodeList::iterator node_iter, iter;
  bool attempt_succeeded;
  TryBlockList::iterator try_iter = _try_blocks.begin();
  for (int i = 0; i < _n_try_blocks_processed; ++i, ++try_iter) {
    ;
  }
  for (; _n_try_blocks_processed < static_cast<int>(_try_blocks.size()); ++try_iter) {
    ++_n_try_blocks_processed;
    attempt_succeeded = true;
    for (node_iter = try_iter->attempt_nodes.begin(); node_iter != try_iter->attempt_nodes.end(); ++node_iter) {
      if ((node_iter->type == DocNode::TYPE_INCLUDE) || (node_iter->type == DocNode::TYPE_SPECIAL_INCLUDE)) {
        const Attribute &url = (*node_iter).attr_list.front();
        string raw_url(url.value, url.value_len);
        if (!_getIncludeData(*node_iter)) {
          attempt_succeeded = false;
          TSError("[%s] attempt section errored; due to url [%s]", __FUNCTION__, raw_url.c_str());
          break;
        }
      }
    }

    if (attempt_succeeded) {
      DBG("[%s] attempt section succeeded; using attempt section", __FUNCTION__);
      _node_list.splice(try_iter->pos, try_iter->attempt_nodes);
    } else {
      DBG("[%s] attempt section errored; trying except section", __FUNCTION__);
      int n_prescanned_nodes = 0;
      if (!_preprocess(try_iter->except_nodes, n_prescanned_nodes)) {
        TSError("[%s] Failed to preprocess except nodes", __FUNCTION__);
        stop();
        return FAILURE;
      }
      _node_list.splice(try_iter->pos, try_iter->except_nodes);
      if (_fetcher.getNumPendingRequests()) {
        DBG("[%s] New fetch requests were triggered by except block; "
            "Returning NEED_MORE_DATA...",
            __FUNCTION__);
        return NEED_MORE_DATA;
      }
    }
  }
  _curr_state = PROCESSED;
  for (node_iter = _node_list.begin(); node_iter != _node_list.end(); ++node_iter) {
    DocNode &doc_node = *node_iter; // handy reference
    DBG("[%s] Processing ESI node [%s] with data of size %d starting with [%.10s...]", __FUNCTION__,
        DocNode::type_names_[doc_node.type], doc_node.data_len, (doc_node.data_len ? doc_node.data : "(null)"));
    if (doc_node.type == DocNode::TYPE_PRE) {
      // just copy the data
      _output_data.append(doc_node.data, doc_node.data_len);
    } else if (!_processEsiNode(node_iter)) {
      TSError("[%s] Failed to process ESI node [%.*s]", __FUNCTION__, doc_node.data_len, doc_node.data);
      stop();
      return FAILURE;
    }
  }
  _addFooterData();
  data     = _output_data.c_str();
  data_len = _output_data.size();
  DBG("[%s] ESI processed document of size %d starting with [%.10s]", __FUNCTION__, data_len, (data_len ? data : "(null)"));
  return SUCCESS;
}

EsiProcessor::ReturnCode
EsiProcessor::flush(string &data, int &overall_len)
{
  if (_curr_state == ERRORED) {
    return FAILURE;
  }
  if (_curr_state == PROCESSED) {
    overall_len = _overall_len;
    data.assign("");
    return SUCCESS;
  }
  DocNodeList::iterator node_iter, iter;
  bool attempt_succeeded;
  bool attempt_pending;
  bool node_pending;
  _output_data.clear();
  TryBlockList::iterator try_iter = _try_blocks.begin();
  for (int i = 0; i < _n_try_blocks_processed; ++i, ++try_iter) {
    ;
  }
  for (; _n_try_blocks_processed < static_cast<int>(_try_blocks.size()); ++try_iter) {
    attempt_pending = false;
    for (node_iter = try_iter->attempt_nodes.begin(); node_iter != try_iter->attempt_nodes.end(); ++node_iter) {
      if ((node_iter->type == DocNode::TYPE_INCLUDE) || (node_iter->type == DocNode::TYPE_SPECIAL_INCLUDE)) {
        if (_getIncludeStatus(*node_iter) == STATUS_DATA_PENDING) {
          attempt_pending = true;
          break;
        }
      }
    }
    if (attempt_pending) {
      break;
    }

    ++_n_try_blocks_processed;
    attempt_succeeded = true;
    for (node_iter = try_iter->attempt_nodes.begin(); node_iter != try_iter->attempt_nodes.end(); ++node_iter) {
      if ((node_iter->type == DocNode::TYPE_INCLUDE) || (node_iter->type == DocNode::TYPE_SPECIAL_INCLUDE)) {
        const Attribute &url = (*node_iter).attr_list.front();
        string raw_url(url.value, url.value_len);
        if (_getIncludeStatus(*node_iter) != STATUS_DATA_AVAILABLE) {
          attempt_succeeded = false;
          TSError("[%s] attempt section errored; due to url [%s]", __FUNCTION__, raw_url.c_str());
          break;
        }
      }
    }

    if (attempt_succeeded) {
      DBG("[%s] attempt section succeeded; using attempt section", __FUNCTION__);
      _n_prescanned_nodes = _n_prescanned_nodes + try_iter->attempt_nodes.size();
      _node_list.splice(try_iter->pos, try_iter->attempt_nodes);
    } else {
      DBG("[%s] attempt section errored; trying except section", __FUNCTION__);
      int n_prescanned_nodes = 0;
      if (!_preprocess(try_iter->except_nodes, n_prescanned_nodes)) {
        TSError("[%s] Failed to preprocess except nodes", __FUNCTION__);
      }
      _n_prescanned_nodes = _n_prescanned_nodes + try_iter->except_nodes.size();
      _node_list.splice(try_iter->pos, try_iter->except_nodes);
      if (_fetcher.getNumPendingRequests()) {
        DBG("[%s] New fetch requests were triggered by except block; "
            "Returning NEED_MORE_DATA...",
            __FUNCTION__);
      }
    }
  }

  node_pending = false;
  node_iter    = _node_list.begin();
  for (int i = 0; i < _n_processed_nodes; ++i, ++node_iter) {
    ;
  }
  for (; node_iter != _node_list.end(); ++node_iter) {
    DocNode &doc_node = *node_iter; // handy reference
    DBG("[%s] Processing ESI node [%s] with data of size %d starting with [%.10s...]", __FUNCTION__,
        DocNode::type_names_[doc_node.type], doc_node.data_len, (doc_node.data_len ? doc_node.data : "(null)"));

    if (_getIncludeStatus(doc_node) == STATUS_DATA_PENDING) {
      node_pending = true;
      break;
    }

    DBG("[%s] processed node: %d, try blocks processed: %d, processed try nodes: %d", __FUNCTION__, _n_processed_nodes,
        _n_try_blocks_processed, _n_processed_try_nodes);
    if (doc_node.type == DocNode::TYPE_TRY) {
      if (_n_try_blocks_processed <= _n_processed_try_nodes) {
        node_pending = true;
        break;
      } else {
        ++_n_processed_try_nodes;
      }
    }

    DBG("[%s] really Processing ESI node [%s] with data of size %d starting with [%.10s...]", __FUNCTION__,
        DocNode::type_names_[doc_node.type], doc_node.data_len, (doc_node.data_len ? doc_node.data : "(null)"));

    if (doc_node.type == DocNode::TYPE_PRE) {
      // just copy the data
      _output_data.append(doc_node.data, doc_node.data_len);
      ++_n_processed_nodes;
    } else if (!_processEsiNode(node_iter)) {
      TSError("[%s] Failed to process ESI node [%.*s]", __FUNCTION__, doc_node.data_len, doc_node.data);
      ++_n_processed_nodes;
    } else {
      ++_n_processed_nodes;
    }
  }

  if (!node_pending && (_curr_state == WAITING_TO_PROCESS)) {
    _curr_state = PROCESSED;
    _addFooterData();
  }
  data.assign(_output_data);
  _overall_len = _overall_len + data.size();
  overall_len  = _overall_len;

  DBG("[%s] ESI processed document of size %d starting with [%.10s]", __FUNCTION__, int(data.size()),
      (data.size() ? data.data() : "(null)"));
  return SUCCESS;
}

void
EsiProcessor::stop()
{
  _output_data.clear();
  _node_list.clear();
  _include_urls.clear();
  _try_blocks.clear();
  _n_prescanned_nodes     = 0;
  _n_try_blocks_processed = 0;
  _overall_len            = 0;
  for (IncludeHandlerMap::iterator map_iter = _include_handlers.begin(); map_iter != _include_handlers.end(); ++map_iter) {
    delete map_iter->second;
  }
  _include_handlers.clear();
  _curr_state = STOPPED;
}

EsiProcessor::~EsiProcessor()
{
  if (_curr_state != STOPPED) {
    stop();
  }
}

bool
EsiProcessor::_processEsiNode(const DocNodeList::iterator &iter)
{
  bool retval;
  const DocNode &node = *iter;
  if ((node.type == DocNode::TYPE_INCLUDE) || (node.type == DocNode::TYPE_SPECIAL_INCLUDE)) {
    const char *content;
    int content_len;
    if ((retval = _getIncludeData(node, &content, &content_len))) {
      if (content_len > 0) {
        _output_data.append(content, content_len);
      }
    }
  } else if ((node.type == DocNode::TYPE_COMMENT) || (node.type == DocNode::TYPE_REMOVE) || (node.type == DocNode::TYPE_TRY) ||
             (node.type == DocNode::TYPE_CHOOSE) || (node.type == DocNode::TYPE_HTML_COMMENT)) {
    // choose, try and html-comment would've been dealt with earlier
    DBG("[%s] No-op for [%s] node", __FUNCTION__, DocNode::type_names_[node.type]);
    retval = true;
  } else if (node.type == DocNode::TYPE_VARS) {
    retval = _handleVars(node.data, node.data_len);
  } else {
    TSError("[%s] Unknown ESI Doc node type %d", __FUNCTION__, node.type);
    retval = false;
  }
  if (retval) {
    DBG("[%s] Processed ESI [%s] node", __FUNCTION__, DocNode::type_names_[node.type]);
  } else {
    TSError("[%s] Failed to process ESI doc node of type %d", __FUNCTION__, node.type);
  }
  return retval;
}

inline bool
EsiProcessor::_isWhitespace(const char *data, int data_len)
{
  for (int i = 0; i < data_len; ++i) {
    if (!isspace(data[i])) {
      return false;
    }
  }
  return true;
}

bool
EsiProcessor::_handleChoose(DocNodeList::iterator &curr_node)
{
  DocNodeList::iterator iter, otherwise_node, winning_node, end_node;
  end_node       = curr_node->child_nodes.end();
  otherwise_node = end_node;
  for (iter = curr_node->child_nodes.begin(); iter != end_node; ++iter) {
    if (iter->type == DocNode::TYPE_OTHERWISE) {
      otherwise_node = iter;
      break;
    }
  }
  winning_node = end_node;
  for (iter = curr_node->child_nodes.begin(); iter != end_node; ++iter) {
    if (iter->type == DocNode::TYPE_WHEN) {
      const Attribute &test_expr = iter->attr_list.front();
      if (_expression.evaluate(test_expr.value, test_expr.value_len)) {
        winning_node = iter;
        break;
      }
    }
  }
  if (winning_node == end_node) {
    DBG("[%s] All when nodes failed to evaluate to true", __FUNCTION__);
    if (otherwise_node != end_node) {
      DBG("[%s] Using otherwise node...", __FUNCTION__);
      winning_node = otherwise_node;
    } else {
      DBG("[%s] No otherwise node, nothing to do...", __FUNCTION__);
      return true;
    }
  }
  // splice() inserts elements *before* given position, but we need to
  // insert new nodes after the choose node for them to be seen by
  // preprocess(); hence...
  DocNodeList::iterator next_node = curr_node;
  ++next_node;
  _node_list.splice(next_node, winning_node->child_nodes);
  return true;
}

bool
EsiProcessor::_handleTry(DocNodeList::iterator &curr_node)
{
  DocNodeList::iterator iter, end_node = curr_node->child_nodes.end();
  DocNodeList::iterator attempt_node = end_node, except_node = end_node;
  for (iter = curr_node->child_nodes.begin(); iter != end_node; ++iter) {
    if (iter->type == DocNode::TYPE_ATTEMPT) {
      attempt_node = iter;
    } else if (iter->type == DocNode::TYPE_EXCEPT) {
      except_node = iter;
    }
  }
  TryBlock try_info(attempt_node->child_nodes, except_node->child_nodes, curr_node);
  int n_prescanned_nodes = 0;
  if (!_preprocess(try_info.attempt_nodes, n_prescanned_nodes)) {
    TSError("[%s] Couldn't preprocess attempt node of try block", __FUNCTION__);
    return false;
  }
  _try_blocks.push_back(try_info);
  return true;
}

bool
EsiProcessor::_handleVars(const char *str, int str_len)
{
  const string &str_value = _expression.expand(str, str_len);
  DBG("[%s] Vars expression [%.*s] expanded to [%.*s]", __FUNCTION__, str_len, str, int(str_value.size()), str_value.data());
  _output_data.append(str_value);
  return true;
}

bool
EsiProcessor::_handleHtmlComment(const DocNodeList::iterator &curr_node)
{
  DocNodeList inner_nodes;
  if (!_parser.parse(inner_nodes, curr_node->data, curr_node->data_len)) {
    TSError("[%s] Couldn't parse html comment node content", __FUNCTION__);
    Stats::increment(Stats::N_PARSE_ERRS);
    return false;
  }
  DBG("[%s] parsed %d inner nodes from html comment node", __FUNCTION__, int(inner_nodes.size()));
  DocNodeList::iterator next_node = curr_node;
  ++next_node;
  _node_list.splice(next_node, inner_nodes); // insert after curr node for pre-processing
  return true;
}

bool
EsiProcessor::_preprocess(DocNodeList &node_list, int &n_prescanned_nodes)
{
  DocNodeList::iterator list_iter = node_list.begin();
  StringHash::iterator hash_iter;
  string raw_url;

  // skip previously examined nodes
  for (int i = 0; i < n_prescanned_nodes; ++i, ++list_iter) {
    ;
  }

  for (; list_iter != node_list.end(); ++list_iter, ++n_prescanned_nodes) {
    switch (list_iter->type) {
    case DocNode::TYPE_CHOOSE:
      if (!_handleChoose(list_iter)) {
        TSError("[%s] Failed to preprocess choose node", __FUNCTION__);
        return false;
      }
      DBG("[%s] handled choose node successfully", __FUNCTION__);
      break;
    case DocNode::TYPE_TRY:
      if (!_handleTry(list_iter)) {
        TSError("[%s] Failed to preprocess try node", __FUNCTION__);
        return false;
      }
      DBG("[%s] handled try node successfully", __FUNCTION__);
      break;
    case DocNode::TYPE_HTML_COMMENT:
      /**
       * the html comment <!--esi inner text--> is a container.
       * the esi processor will remove the starting tag "<!--esi" and the
       * closure tag "-->", then keep the inner text (the content within it).
       *
       * we should call _handleHtmlComment when the node list is parsed
       * from the content,
       * but we should NOT call _handleHtmlComment again when the node list
       * is unpacked from the cache because the node list has been parsed.
       */
      if (!_usePackedNodeList && !_handleHtmlComment(list_iter)) {
        TSError("[%s] Failed to preprocess try node", __FUNCTION__);
        return false;
      }
      break;
    case DocNode::TYPE_INCLUDE: {
      Stats::increment(Stats::N_INCLUDES);
      const Attribute &src = list_iter->attr_list.front();
      raw_url.assign(src.value, src.value_len);
      DBG("[%s] Adding fetch request for url [%.*s]", __FUNCTION__, int(raw_url.size()), raw_url.data());
      hash_iter = _include_urls.find(raw_url);
      if (hash_iter != _include_urls.end()) { // we have already processed this URL
        DBG("[%s] URL [%.*s] already processed", __FUNCTION__, int(raw_url.size()), raw_url.data());
        continue;
      }
      const string &expanded_url = _expression.expand(raw_url);
      if (!expanded_url.size()) {
        TSError("[%s] Couldn't expand raw URL [%.*s]", __FUNCTION__, int(raw_url.size()), raw_url.data());
        Stats::increment(Stats::N_INCLUDE_ERRS);
        continue;
      }

      if (!_fetcher.addFetchRequest(expanded_url)) {
        TSError("[%s] Couldn't add fetch request for URL [%.*s]", __FUNCTION__, int(raw_url.size()), raw_url.data());
        Stats::increment(Stats::N_INCLUDE_ERRS);
        continue;
      }
      _include_urls.insert(StringHash::value_type(raw_url, expanded_url));
      break;
    }
    case DocNode::TYPE_SPECIAL_INCLUDE: {
      Stats::increment(Stats::N_SPCL_INCLUDES);
      const Attribute &handler_attr = list_iter->attr_list.front();
      string handler_id(handler_attr.value, handler_attr.value_len);
      SpecialIncludeHandler *handler;
      IncludeHandlerMap::const_iterator map_iter = _include_handlers.find(handler_id);
      if (map_iter == _include_handlers.end()) {
        handler = _handler_manager.getHandler(_esi_vars, _expression, _fetcher, handler_id);
        if (!handler) {
          TSError("[%s] Couldn't create handler with id [%s]", __FUNCTION__, handler_id.c_str());
          Stats::increment(Stats::N_SPCL_INCLUDE_ERRS);
          return false;
        }
        _include_handlers.insert(IncludeHandlerMap::value_type(handler_id, handler));
        DBG("[%s] Created new special include handler object for id [%s]", __FUNCTION__, handler_id.c_str());
      } else {
        handler = map_iter->second;
      }
      int special_data_id = handler->handleInclude(list_iter->data, list_iter->data_len);
      if (special_data_id == -1) {
        TSError("[%s] Include handler [%s] couldn't process include with data [%.*s]", __FUNCTION__, handler_id.c_str(),
                list_iter->data_len, list_iter->data);
        Stats::increment(Stats::N_SPCL_INCLUDE_ERRS);
        return false;
      }
      // overloading this structure's members
      // handler will be in value and include id will be in value_len of the structure
      list_iter->attr_list.push_back(Attribute(INCLUDE_DATA_ID_ATTR, 0, reinterpret_cast<const char *>(handler), special_data_id));
      DBG("[%s] Got id %d for special include at node %d from handler [%s]", __FUNCTION__, special_data_id, n_prescanned_nodes + 1,
          handler_id.c_str());
    } break;
    default:
      break;
    }
  }

  return true;
}

void
EsiProcessor::_addFooterData()
{
  const char *footer;
  int footer_len;
  for (IncludeHandlerMap::iterator iter = _include_handlers.begin(); iter != _include_handlers.end(); ++iter) {
    iter->second->getFooter(footer, footer_len);
    if (footer_len > 0) {
      _output_data.append(footer, footer_len);
    }
  }
}
