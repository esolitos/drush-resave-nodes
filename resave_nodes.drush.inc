<?php

function resave_nodes_drush_command() {
  $items  = array();
  $items['resave-nodes'] = array(
    'description' => dt('Re-save nodes example'),
    'aliases'     => array('rn'),
    'command-hook'    => 'setup',
    'required-arguments' => TRUE,
    'arguments'   => array(
      'action' => "Specify which action should be done, by default only 'example' is available",
    ),
    'options' => array(
      'node-list'    => array(
        'description'   => "Comma separated list of Node IDs to process. Note: if specified the options bundle, limit and offset are ignored",
        'value'         => 'required',
        'example-value' => '3,75,86,158',
      ),
      'limit-bundle'    => array(
        'description'   => "Limit the action to a specific node type",
        'value'         => 'required',
        'example-value' => 'page',
      ),
      'max'    => array(
        'description'   => "Limit the number of nodes to process",
        'value'         => 'required',
        'example-value' => '150',
      ),
      'skip'    => array(
        'description'   => "Skips the given number of node IDs",
        'value'         => 'required',
        'example-value' => '10',
      ),
    ),
  );

  return $items;
}
 
function resave_nodes_drush_help($section) {
  switch ($section) {
    case 'drush:resave-nodes':
      return dt("Applies an action to all (or some) nodes providing some useful defaults to extend the module.");
  }
}

/**
 * 
 */
function drush_resave_nodes_setup($required_action) {
  $defined_actions = array();
  foreach (new DirectoryIterator(__DIR__ . '/actions') as $file) {
    $plugin = FALSE;
    if ( $file->isFile() && $file->getExtension() == 'inc' ) {
      require_once($file->getPathname());
      // Only act if the plugin var has been defined
      if ( isset($plugin['action']) ) {
        if ( empty($plugin['file']) ) {
          $plugin['file'] = $file->getPathname();
        }

        $defined_actions[ $plugin['action'] ] = $plugin;
      }
    }
  }

  if ( isset($defined_actions[$required_action]) ) {
    $function = $defined_actions[$required_action]['callback'];
    $extra_args = isset($defined_actions[$required_action]['arguments']) ? $defined_actions[$required_action]['callback'] : NULL;

    $node_list         = drush_get_option('node-list', NULL);
    $filters['bundle'] = drush_get_option('limit-bundle', FALSE);
    $filters['limit']  = drush_get_option('max', 0);
    $filters['offset'] = drush_get_option('skip', 0);
    $action_plugin     = $defined_actions[$required_action];

    // Setup the batch
    resave_nodes_setup_batch($action_plugin, $node_list, $filters);
  }
  else {
   drush_log(strtr('Te action @action is not implemented!', array('@action'=>$action)), 'error');
  }
}

/**
 * Example function to setup a batch and run it
 */
function resave_nodes_setup_batch($action_plugin, $nid_list = NULL, $filters = array()) {
  $node_ids = ( !$nid_list ) ? _resave_nodes_get_nid_list($filters) : explode(',', $nid_list);

  batch_set(array(
    'operations' => _resave_nodes_setup_operations($node_ids, $action_plugin),
    'title' => strtr("Resave Nodes: @action_info", array('@action_info'=>$action_plugin['description'])),
  ));
  $batch =& batch_get();

  //Because we are doing this on the back-end, we set progressive to false.
  $batch['progressive'] = FALSE;
  //Start processing the batch operations.
  drush_backend_batch_process();
}

/**
 * Gets some node ids based on node type and optional limits
 */
function _resave_nodes_get_nid_list(array $filters) {
  $nodes_query = db_select('node', 'n')
    ->fields('n', array('nid'));

  if ( !empty($filters['bundle']) ) {
    $nodes_query->condition('type', $node_type);
  }

  if ( !empty($filters['offset']) || !empty($filters['limit']) ) {
    $nodes_query->range($filters['offset'], $filters['limit']);
  }

  $nodes_query->orderBy('nid', 'ASC');

  return (array) $nodes_query->execute()->fetchCol();
}

/**
 * Creates an array of operations based on a node list and a callback function
 *
 * @param array $nid_list  List of node IDs
 * @param plugin with name reference to callback function that will be called during the process
 * @param Any extra parameter passed to this function will be passed to the callback function!
 */
function _resave_nodes_setup_operations(array $nid_list, $action_plugin) {
  // Increase memory limit and enable garbage collector
  @ini_set('memory_limit', -1);
  gc_enable();
  
  //Break up all of our data so each process does not time out.
  $chunks = array_chunk($nid_list, 50);
  $operations = $extra_params = array();
  $count_chunks = count($chunks);

  /* NOT YET SUPPORTED */
  if ( func_num_args() > 2 ) {
    $extra_params = func_get_args();
    // Discard the first two default arguments
    array_shift($extra_params);
    array_shift($extra_params);
  }

  for ($i=0; $i < $count_chunks; $i++) {
    $operations[] = array(
      'resave_nodes_batch_run_callback',
      array(
        'data_chunk'         => $chunks[$i],
        'operation_callback' => $action_plugin['callback'],
        'action_file'        => $action_plugin['file'],
      ) + $extra_params,
    );
  }

  return $operations;
}

/**
 * Loads several nodes and run a callback function
 *
 * @param array $data_chuck  List of node IDs
 * @param callback $operation_callback Name of the function to be called during the process
 * @param string $action_file file name of file containing the callback function
 * @param Any extra parameter passed to this function will be passed to the callback function!
 */
function resave_nodes_batch_run_callback($data_chunk, $operation_callback, $action_file) {
  $args = func_get_args();
  $context = array_pop( $args );

  /* NOT YET SUPPORTED */
  if ( count($args) > 3 ) {
    $extra_callback_args = $args;
    // Remove default arguments
    array_shift($extra_callback_args);
    array_shift($extra_callback_args);
    array_shift($extra_callback_args);
  }
  
  // Include the file where the action is define and check if the specified function name is an actuall callable
  require_once $action_file;
  if ( !is_callable($operation_callback) ) {
    drush_log(strtr('The callback function "@callback" does not exist!', array('@callback'=>$operation_callback)), 'error');
    exit;
  }

  foreach ($data_chunk as $nid) {
    drush_log(strtr('Processing Node: @nid', array('@nid'=>$nid)), 'debug');

    $node_wrapper = entity_metadata_wrapper('node', $nid);
    /* Load and check if successfully loaded a node */
    if ( ! $node_wrapper->raw() ) {
      drush_log(strtr('Unable to load node @nid', array('@nid'=>$nid)), 'warning');
    }
    else {
      // Call the actual function that will do the magic!
      drush_log('Calling final callback internal function.', 'notice');
      if(isset($extra_callback_args)) {
        $callback_succeded = $operation_callback($node_wrapper, $extra_callback_args);
      } else {
        $callback_succeded = $operation_callback($node_wrapper);
      }
      
      // Save only if the callback says that it's all good
      if ( $callback_succeded ) {
        $node_wrapper->save();
      } else {
        drush_log('Node not saved.', 'notice');
      }
    }

    // Hope to clean some memory
    $node_wrapper = $node = NULL;
    unset($node, $node_wrapper);
  }
  
  drush_log( strtr('Memory usage: @mem Peak: @peak)', array('@mem'=> _memConvert(memory_get_usage(true)), '@peak'=> _memConvert(memory_get_peak_usage(true))) ), 'debug');
}

/**
 * Converts Bytes in human-readable sizes.
 */
function _memConvert($size) {
  $unit = array('B','KB','MB','GB','TB','PB');
  return @round($size/pow(1024,($i=floor(log($size,1024)))),2).' '.$unit[$i];
}