<?php
// $Id$

/**
 * @file
 * CTools plugins declarations.
 */

/**
 * Break out for feeds_feed_plugins().
 */
function _feeds_feeds_plugins() {
  $path = drupal_get_path('module', 'feeds') .'/plugins';

  $info = array();
  $info['FeedsFetcher'] = array(
    'hidden' => TRUE,
    'handler' => array(
      'class' => 'FeedsFetcher',
      'file' => 'FeedsFetcher.inc',
      'path' => $path,
    ),
  );
  $info['FeedsParser'] = array(
    'hidden' => TRUE,
    'handler' => array(
      'class' => 'FeedsParser',
      'file' => 'FeedsParser.inc',
      'path' => $path,
    ),
  );
  $info['FeedsProcessor'] = array(
    'hidden' => TRUE,
    'handler' => array(
      'class' => 'FeedsProcessor',
      'file' => 'FeedsProcessor.inc',
      'path' => $path,
    ),
  );
  $info['FeedsHttpFetcher'] = array(
    'name' => 'HTTP Fetcher',
    'description' => 'Download content from a URL.',
    'handler' => array(
      'parent' => 'FeedsFetcher', // This is the key name, not the class name.
      'class' => 'FeedsHttpFetcher',
      'file' => 'FeedsHttpFetcher.inc',
      'path' => $path,
    ),
  );
  $info['FeedsFileFetcher'] = array(
    'name' => 'File fetcher',
    'description' => 'Upload content from a local file.',
    'handler' => array(
      'parent' => 'FeedsHttpFetcher',
      'class' => 'FeedsFileFetcher',
      'file' => 'FeedsFileFetcher.inc',
      'path' => $path,
    ),
  );
  $info['FeedsCSVParser'] = array(
    'name' => 'CSV parser',
    'description' => 'Parse data in Comma Separated Value format.',
    'handler' => array(
      'parent' => 'FeedsParser',
      'class' => 'FeedsCSVParser',
      'file' => 'FeedsCSVParser.inc',
      'path' => $path,
    ),
  );
  $info['FeedsSyndicationParser'] = array(
    'name' => 'Common syndication parser',
    'description' => 'Parse XML feeds in RSS 1, RSS 2 or Atom format.',
    'handler' => array(
      'parent' => 'FeedsParser',
      'class' => 'FeedsSyndicationParser',
      'file' => 'FeedsSyndicationParser.inc',
      'path' => $path,
    ),
  );
  $info['FeedsNodeProcessor'] = array(
    'name' => 'Node processor',
    'description' => 'Create nodes from parsed content.',
    'handler' => array(
      'parent' => 'FeedsProcessor',
      'class' => 'FeedsNodeProcessor',
      'file' => 'FeedsNodeProcessor.inc',
      'path' => $path,
    ),
  );
  $info['FeedsDataProcessor'] = array(
    'name' => 'Data processor',
    'description' => 'Create data table records from parsed content.',
    'handler' => array(
      'parent' => 'FeedsProcessor',
      'class' => 'FeedsDataProcessor',
      'file' => 'FeedsDataProcessor.inc',
      'path' => $path,
    ),
  );
  return $info;
}