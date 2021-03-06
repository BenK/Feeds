<?php
// $Id: FeedsBatch.inc,v 1.17 2010/09/20 15:41:03 alexb Exp $

// Batch stages.
define('FEEDS_FETCHING', 'fetching');
define('FEEDS_PARSING', 'parsing');
define('FEEDS_PROCESSING', 'processing');
define('FEEDS_CLEARING', 'clearing');

/**
 * A FeedsBatch object holds the state of an import or clear batch.
 *
 * Used in FeedsSource class. Counter variables are public for easier access.
 */
class FeedsBatch {
  // Total of each stage of this batch.
  protected $total;
  // Progress of each stage of this batch.
  protected $progress;
  public function __construct() {
    $this->total = array();
    $this->progress = array();
  }

  /**
   * Set the total for a stage.
   */
  public function setTotal($stage, $total) {
    $this->total[$stage] = $total;
  }

  /**
   * Get the total for a stage.
   */
  public function getTotal($stage) {
    return $this->total[$stage];
  }

  /**
   * Set progress for a stage.
   *
   * @param $stage
   *   The stage to set the progress for. One of FEEDS_FETCHING, FEEDS_PARSING,
   *   FEEDS_PROCESING or FEEDS_CLEARING.
   * @param $progress
   *   The number of items worked off for the given stage. This should be the
   *   number of items worked off across all page loads, not just the present
   *   page load.
   */
  public function setProgress($stage, $progress) {
    $this->progress[$stage] = $progress;
  }

  /**
   * Report progress.
   *
   * @param $stage
   *   The stage to set the progress for. One of FEEDS_FETCHING, FEEDS_PARSING,
   *   FEEDS_PROCESING or FEEDS_CLEARING.
   */
  public function getProgress($stage = NULL) {
    if ($stage) {
      $progress = $this->progress[$stage];
      if ($progress == FEEDS_BATCH_COMPLETE) {
        return FEEDS_BATCH_COMPLETE;
      }
      $total = $this->total[$stage];
    }
    else {
      $complete = TRUE;
      $progress = 0;
      foreach ($this->progress as $p) {
        $progress += $p;
        $complete &= $p == FEEDS_BATCH_COMPLETE;
      }
      if ($complete) {
        return FEEDS_BATCH_COMPLETE;
      }
      $total = array_sum($this->total);
    }
    $progress = (1.0 / $total) * $progress;
    return $progress == FEEDS_BATCH_COMPLETE ? 0.999 : $progress;
  }
}

/**
 * A FeedsImportBatch wraps the actual content retrieved from a FeedsSource. On
 * import, it is created on the fetching stage and passed through the parsing
 * and processing stage where it is normalized and consumed.
 *
 * A Fetcher must return a FeedsImportBatch object on fetch(). To that end it
 * can use one of the existing FeedsImportBatch classes (FeedsImportBatch,
 * FeedsFileBatch or FeedsHTTPBatch) or provide its own as a direct or indirect
 * extension of FeedsImportBatch.
 *
 * A Parser must populate a FeedsImportBatch object through the set methods upon
 * parse(). For instance:
 *
 * @code
 * $batch->setItems($parsed_rows);
 * $batch->setTitle('My imported document');
 * @endcode
 *
 * Finally, a processor can work off the information produced on the parsing
 * stage by consuming items with $batch->shiftItem().
 *
 * @code
 * while ($item = $batch->shiftItem()) {
 *   $object = $this->map($item);
 *   $object->save();
 * }
 * @endcode
 *
 * If a processing task is very slow, it can be batched over multiple page
 * loads. For batching the consumer loop can be left while the current progress
 * is set on the batch object. If the current progress is not
 * FEEDS_BATCH_COMPLETE the processor will be called again on a subsequent page
 * load to continue where it has left off. For an example, see
 * FeedsNodeProcessor::process().
 *
 * @code
 * $created = 0;
 * while ($item = $batch->shiftItem()) {
 *   $object = $this->map($item);
 *   $object->save();
 *   $created++; // Created in this page load.
 *   $batch->created++; // Created total.
 *   if ($created > MAX_CREATED) {
 *     $batch->setProgress(FEEDS_PROCESSING, $batch->created);
 *     return;
 *   }
 * }
 * $batch->setProgress(FEEDS_PROCESSING, FEEDS_BATCH_COMPLETE);
 * @endcode
 *
 * Note: Knowledge of the internal structure of a single item in the $items
 * array is managed by the mapping API specified in FeedsParser class and
 * FeedsProcessor class.
 *
 * @see FeedsBatch
 * @see FeedsFileBatch
 * @see FeedsHTTPBatch
 */
class FeedsImportBatch extends FeedsBatch {
  protected $file_path;
  protected $title;
  protected $description;
  protected $link;
  protected $items;
  protected $raw;
  protected $feed_nid;
  protected $current_item;
  public $created;
  public $updated;

  public function __construct($raw = '', $feed_nid = 0) {
    parent::__construct();
    $this->progress = array(
      FEEDS_FETCHING => FEEDS_BATCH_COMPLETE,
      FEEDS_PARSING => FEEDS_BATCH_COMPLETE,
      FEEDS_PROCESSING => FEEDS_BATCH_COMPLETE,
    );
    $this->file_path = NULL;
    $this->title = '';
    $this->description = '';
    $this->link = '';
    $this->items = array();
    $this->raw = $raw;
    $this->feed_nid = $feed_nid;
    $this->created = 0;
    $this->updated = 0;
  }

  /**
   * @return
   *   The raw content from the source as a string.
   *
   * @throws Exception
   *   Extending classes MAY throw an exception if a problem occurred.
   */
  public function getRaw() {
    return $this->raw;
  }

  /**
   * Get a path to a temporary file containing the resource provided by the
   * fetcher.
   *
   * File will be deleted after DRUPAL_MAXIMUM_TEMP_FILE_AGE.
   *
   * @return
   *   A path to a file containing the raw content as a source.
   *
   * @throws Exception
   *   If an unexpected problem occurred.
   */
  public function getFilePath() {
    if (!isset($this->file_path)) {
      $destination = 'public://feeds';
      if (!file_prepare_directory($destination, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)) {
        throw new Exception(t('Feeds directory either cannot be created or is not writable.'));
      }
      $this->file_path = FALSE;
      if ($file = file_save_data($this->getRaw(), $destination . '/'. get_class($this) . REQUEST_TIME)) {
        $file->status = 0;
        file_save($file);
        $this->file_path = $file->uri;
      }
      else {
        throw new Exception(t('Cannot write content to %dest', array('%dest' => $destination)));
      }
    }
    return $this->file_path;
  }

  /**
   * Return the feed node related to this batch object.
   */
  public function feedNode() {
    if ($this->feed_nid) {
      return node_load($this->feed_nid);
    }
  }

  /**
   * @return
   *   A string that is the feed's title.
   */
  public function getTitle() {
    return $this->title;
  }

  /**
   * @return
   *   A string that is the feed's description.
   */
  public function getDescription() {
    return $this->description;
  }

  /**
   * @return
   *   A string that is the link to the feed's site (not the actual URL of the
   *   feed). Falls back to URL if not available.
   */
  public function getLink() {
    return $this->link;
  }

  /**
   * @todo Move to a nextItem() based approach, not consuming the item array.
   *   Can only be done once we don't cache the entire batch object between page
   *   loads for batching anymore.
   *
   * @return
   *   Next available item or NULL if there is none. Every returned item is
   *   removed from the internal array.
   */
  public function shiftItem() {
    $this->current_item = array_shift($this->items);
    return $this->current_item;
  }

  /**
   * @return
   *   Current feed item.
   */
  public function currentItem() {
    return empty($this->current_item) ? NULL : $this->current_item;
  }

  /**
   * Set title.
   */
  public function setTitle($title) {
    $this->title = $title;
  }

  /**
   * Set description.
   */
  public function setDescription($description) {
    $this->description = $description;
  }

  /**
   * Set link.
   */
  public function setLink($link) {
    $this->link = $link;
  }

  /**
   * Set items.
   *
   * @param $items
   *   An array of the items in the feed. Cannot be NULL.
   */
  public function setItems($items) {
    $this->items = $items;
  }

  /**
   * Add an item.
   */
  public function addItem($item) {
    $this->items[] = $item;
  }

  /**
   * Get number of items.
   */
  public function getItemCount() {
    return count($this->items);
  }
}

/**
 * Batch class for batched deleting of items.
 */
class FeedsClearBatch extends FeedsBatch {
  // Number of items deleted.
  public $deleted;
  public function __construct() {
    parent::__construct();
    $this->progress = array(
      FEEDS_CLEARING => FEEDS_BATCH_COMPLETE,
    );
    $this->deleted = 0;
  }
}
