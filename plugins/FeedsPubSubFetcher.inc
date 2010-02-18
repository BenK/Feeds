<?php

feeds_include_library('PushSubscriber.inc', 'PushSubscriber');

/**
 * FeedsImportBatch for PubSub Fetcher.
 */
class FeedsPubSubBatch extends FeedsHTTPBatch {
  protected $raw;

  /**
   * Constructor
   *
   * @param $raw
   *   The raw content of the fat ping.
   */
  function __construct($raw) {
    $this->raw = $raw;
    parent::__construct();
  }

  /**
   * Override FeedsHTTPBatch::getRaw() and simply return the raw content
   * populated on instnatiation.
   */
  function getRaw() {
    return $this->raw;
  }
}

/**
 * Publish/Subscribe fetcher. Supports at the moment only PubSubHubbub (PuSH).
 */
class FeedsPubSubFetcher extends FeedsHTTPFetcher {

  /**
   * Implementation of FeedsFetcher::fetch().
   */
  public function fetch(FeedsSource $source) {
    $source_config = $source->getConfigFor($this);
    // Handle fat ping if present, otherwise pass up to HTTP fetcher.
    if ($raw = feeds_push_subscriber($this->id, $source->feed_nid)->receive()) {
      return new FeedsPubSubBatch($raw);
    }
    return parent::fetch($source);
  }

  /**
   * Override sourceSave() - subscribe to hub.
   */
  public function sourceSave(FeedsSource $source) {
    $this->subscribe($source);
  }

  /**
   * Override sourceDelete() - unsubscribe from hub.
   */
  public function sourceDelete(FeedsSource $source) {
    $this->unsubscribe($source);
  }

  /**
   * Implement FeedsFetcher::subscribe() - subscribe to hub.
   *
   * @todo Abstract out KeyAuth integration, this is not at all pub/sub
   * specific.
   */
  public function subscribe(FeedsSource $source) {
    $source_config = $source->getConfigFor($this);
    $url = $source_config['source'];
    if (module_exists('keyauth') && $this->config['use_keyauth']) {
      keyauth_include();
      $url = keyauth_sign_url($this->config['keyauth_public'], $url);
    }
    feeds_push_subscriber($this->id, $source->feed_nid)->subscribe($url, url('feeds/pubsubhubbub/notify/'. $this->id .'/'. $source->feed_nid, array('absolute' => TRUE)), valid_url($this->config['designated_hub']) ? $this->config['designated_hub'] : '');
  }

  /**
   * Implement FeedsFetcher::unsubscribe() - unsubscribe from hub.
   */
  public function unsubscribe(FeedsSource $source) {
    $source_config = $source->getConfigFor($this);
    feeds_push_subscriber($this->id, $source->feed_nid)->unsubscribe($source_config['source'], url('feeds/pubsubhubbub/notify/'. $this->id .'/'. $source->feed_nid, array('absolute' => TRUE)));
  }

  /**
   * Implement FeedsFetcher::subscriptionPeriod().
   * Indicate how often a subscription needs to be renewed.
   *
   * @todo subscription_period should actually be retrieved from the hub's
   *   response to a subscription. This will mean a different subscription
   *   period per source, hence a major change to FeedsScheduler which is
   *   currently assuming a fixed period per task!
   */
  public function subscriptionPeriod() {
    return $this->config['subscription_period'];
  }

  /**
   * Return defaults for configuration.
   */
  public function configDefaults() {
    $defaults = parent::configDefaults();
    return $defaults + array(
      'subscription_period' => 3600*24, // Renew subscription in 24 hours.
      'designated_hub' => '',
    );
  }

  /**
   * Override parent::configForm().
   */
  public function configForm(&$form_state) {
    $form = parent::configForm($form_state);
    $period = drupal_map_assoc(array(0, 900, 1800, 3600, 10800, 21600, 43200, 86400, 259200, 604800, 2419200), 'format_interval');
    $period[FEEDS_SCHEDULE_NEVER] = t('Never renew');
    $period[0] = t('Renew as often as possible');
    $form['subscription_period'] = array(
      '#type' => 'select',
      '#title' => t('Renew subscription after'),
      '#options' => $period,
      '#description' => t('This is the minimum time that must elapse before a subscription is renewed.'),
      '#default_value' => $this->config['subscription_period'],
    );
    $form['designated_hub'] = array(
      '#type' => 'textfield',
      '#title' => t('Designated hub'),
      '#description' => t('Enter the callback URL of a designated hub. If given, this hub will be used instead of the hub specified in the feed source.'),
      '#default_value' => $this->config['designated_hub'],
    );
    return $form;
  }

  /**
   * Verify a PubSubHubbub subscription request.
   */
  public function pushVerifySubscriptionRequest($verify_fields, $feed_nid) {
    feeds_push_subscriber($this->id, $feed_nid)->verifySubscriptionRequest($verify_fields);
  }
}

/**
 * Create a PubSubHubbub subscriber.
 *
 * @return PushSubscriber
 *   A PushSubscriber object.
 */
function feeds_push_subscriber($id, $subscriber_id) {
  return PushSubscriber::instance($id, $subscriber_id, PuSHSubscriptions::instance());
}

/**
 * Implement a PuSHSubscriptionsInterface.
 */
class PuSHSubscriptions implements PuSHSubscriptionsInterface {
  /**
   * Singleton.
   */
  public function instance() {
    static $subscriptions;
    if (empty($subscriptions)) {
      $subscriptions = new PuSHSubscriptions();
    }
    return $subscriptions;
  }

  /**
   * Protect constructor.
   */
  protected function __construct() {
  }

  /**
   * Save a subscription.
   */
  public function save($domain, $subscriber_id, $hub, $topic, $mode, $post_fields = array()) {
    $save = array(
      'domain' => $domain,
      'subscriber_id' => $subscriber_id,
      'hub' => $hub,
      'topic' => $topic,
      'mode' => $mode,
      'post_fields' => $post_fields,
      'timestamp' => time(),
    );
    $this->delete($domain, $subscriber_id);
    drupal_write_record('feeds_push_subscriptions', $save);
  }

  /**
   * Load a subscription.
   */
  public function load($domain, $subscriber_id) {
    if ($request = db_fetch_array(db_query("SELECT * FROM {feeds_push_subscriptions} WHERE domain = '%s' AND subscriber_id = %d", $domain, $subscriber_id))) {
      $request['post_fields'] = unserialize($request['post_fields']);
    }
    return $request;
  }

  /**
   * Delete a subscription.
   */
  public function delete($domain, $subscriber_id) {
    db_query("DELETE FROM {feeds_push_subscriptions} WHERE domain = '%s' AND subscriber_id = %d", $domain, $subscriber_id);
  }
}