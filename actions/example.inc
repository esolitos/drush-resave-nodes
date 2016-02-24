<?php

$plugin = array(
  'action' => "example",
  'description' => "Just print some info",
  'callback' => "EXAMPLE_resave_nodes_callback",
);


/**
 * EXAMPLE: Just print out the title and nid
 */
function EXAMPLE_resave_nodes_callback(EntityMetadataWrapper &$node_wrapper) {
  $vars = array(
    '@nid'   =>$node_wrapper->nid->value(),
    '@title' =>$node_wrapper->title->value(),
  );

  drush_log(strtr('Node: @nid has title: "@title".', $vars), 'ok');
  
  // Do not save this node!
  return FALSE;
}