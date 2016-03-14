<?php

$plugin = array(
  'action' => "meta-canonical",
  'description' => "Save canonical url to meta tag fields",
  'callback' => "meta_canonical_callback",
);

/**
 * Save new pattern to meta tag fields 'canonical' and 'og:url' for provided nod
  * @param EntityMetadataWrapper with node to be processed
 */
function meta_canonical_callback(EntityMetadataWrapper &$node_wrapper) {
  $entity_type = $node_wrapper->type();
  $entity_id   = $node_wrapper->getIdentifier();
  $revision_id = $node_wrapper->vid->value();
  $metatags    = metatag_metatags_load($entity_type, $entity_id);
  if ($metatags) {
    $canonical = $metatags[$node_wrapper->language->value()]['canonical']['value'];
    $og_url    = $metatags[$node_wrapper->language->value()]['og:url']['value'];
  } else {
    $canonical = $og_url = 'inherit';
  }

  $log_vars = array(
    '@nid'   => $node_wrapper->nid->value(),
    '@title' => $node_wrapper->title->value(),
    '@language'  => $node_wrapper->language->value(),
    '@canonical' =>  $canonical,
    '@og_url'    => $og_url,
  );
  $log_message = 'Node: @nid, title: "@title", language: @language, canonical: @canonical, og:url: @og_url.';
  drush_log(strtr($log_message, $log_vars), 'debug');

  $new_canonical = '[current-page:url:absolute]/?test=' . $node_wrapper->changed->value();
  $metatags = array(
    $node_wrapper->language->value() => array(
      'canonical' => array(
        'value' => $new_canonical
      ),
      'og:url' => array(
        'value' => $new_canonical
      ),
    )
  );
  metatag_metatags_save($entity_type, $entity_id, $revision_id, $metatags);

  $log_vars2 = array(
    '@nid'   => $node_wrapper->nid->value(),
    '@canonical' =>  $new_canonical,
  );
  drush_log(strtr("Node: @nid, metatags saved with canonical: @canonical", $log_vars2), 'debug');

  // Do not save this node!
  return FALSE;
}
