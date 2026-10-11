#pragma once

struct item_def;

bool is_stackable_item(const item_def &item);
bool items_similar(const item_def &item1, const item_def &item2);

// The caller supplies identification knowledge so stack compatibility can be
// checked without consulting the current player's identification state.
bool items_stack(const item_def &item1, const item_def &item2,
                 bool first_identified, bool second_identified);

// Compatibility entry point for callers that still use global item knowledge.
bool items_stack(const item_def &item1, const item_def &item2);
