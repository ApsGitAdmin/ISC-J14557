<?php

/**
 * @file
 * Page callbacks for adding, editing and deleting users in the User Manager.
 */

function user_manager_adduser() {
  return drupal_get_form('user_manager_add_form');
}

function user_manager_edituser($account) {
  return drupal_get_form('user_manager_add_form', $account);
}

function user_manager_delete($account) {
  return drupal_get_form('user_manager_delete_form', $account);
}

/**
 * Menu callback for the User Manager main page.
 */
function user_manager_userlist() {
  // Get the user accounts the current user can manage.
  $user_accounts_list = user_manager_get_manageable_users();

  $output = theme('user_manager_userlist', array('accounts' => $user_accounts_list));
  return $output;
}

function user_manager_delete_form($form, &$form_state) {
  $form['user_manager_delete']['confirmation_message'] = array(
    '#markup' => '<div>' . t('Do you wish to delete this user?') . '</div>',
  );
  $form['user_manager_delete']['confirmation_button'] = array(
    '#type' => 'submit',
    '#value' => t('Delete'),
    '#submit' => array('user_manager_delete_user'),
  );
  $form['user_manager_delete']['cancel_button'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#submit' => array('user_manager_return_dashboard'),
  );

  return $form;
}

/**
 * Triggers the deletion of a user through the User Manager.
 */
function user_manager_delete_user($form, &$form_state) {
  $account = $form_state['build_info']['args'][0];

  user_cancel(array(), $account->uid, 'user_cancel_delete');
  // Since user_cancel() is not invoked via Form API, batch processing needs
  // to be invoked manually and should redirect to the front page after
  // completion.
  batch_process('user-manager');
}

function user_manager_add_form($form, &$form_state) {
  if (isset($form_state['build_info']['args'][0])) {
    $account = $form_state['build_info']['args'][0];
  }

  $default_values = array(
    'name' => isset($account->name) ? $account->name : '',
    'mail' => isset($account->mail) ? $account->mail : '',
    'status' => isset($account->status) ? $account->status : 1,
  );

  $form['user_manager_add']['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#required' => TRUE,
    '#default_value' => $default_values['name'],
  );
  $form['user_manager_add']['pass'] = array(
    '#type' => 'password_confirm',
    '#required' => isset($account) ? FALSE : TRUE,
  );
  $form['user_manager_add']['mail'] = array(
    '#type' => 'textfield',
    '#title' => t('Email'),
    '#required' => TRUE,
    '#default_value' => $default_values['mail'],
  );
  $form['user_manager_add']['status'] = array(
    '#type' => 'select',
    '#title' => t('Block user'),
    '#options' => array(
      '1' => t('No'),
      '0' => t('Yes'),
    ),
    '#default_value' => $default_values['status'],
  );
  $form['user_manager_add']['save_button'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  $form['user_manager_add']['cancel_button'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#element_validate' => array('user_manager_return_dashboard'),
  );

  return $form;
}

/**
 * Validate handler for the User Add form.
 */
function user_manager_add_form_validate($form, &$form_state) {
  if (isset($form_state['build_info']['args'][0])) {
    $account = $form_state['build_info']['args'][0];
  }

  $values = $form_state['values'];
  if (!isset($account) && user_load_by_name($values['name'])) {
    form_set_error('name', t('A user with that name already exists.'));
  }
  if (!empty($values['pass']) && strlen($values['pass']) < 6) {
    form_set_error('pass', t('Password needs to be at least 6 characters long.'));
  }
  if (!valid_email_address($values['mail'])) {
    form_set_error('mail', t('The email address given is not valid.'));
  }
  else if (!isset($account) && user_load_by_mail($values['mail'])) {
    form_set_error('mail', t('A user with that email already exists.'));
  }
}

/**
 * Submit handler for the User Add form.
 */
function user_manager_add_form_submit($form, &$form_state) {
  global $user;

  if (isset($form_state['build_info']['args'][0])) {
    $account = $form_state['build_info']['args'][0];
  }

  $values = $form_state['values'];

  // If it's a new account.
  if (!isset($account)) {
    $account = new stdClass();
    $account->is_new = TRUE;
  }
  // Set the chosen status for the account.
  $account->status = $values['status'];

  $edit = array(
    'name' => $values['name'],
    'pass' => $values['pass'],
    'mail' => $values['mail'],
    'signature_format' => 'filtered_html',
  );

  // Assign the current user as the Client Manager owner, so that the account
  // is still manageable by the current Client Manager even before it's been
  // assigned to a sub-group of the client.
  $account->data = array(
    'client_manager_owner' => $user->uid,
  );

  $account = user_save($account, $edit);
//  dpm($account);
  if ($account) {
    // Notify the user of the new / edited account.
    $op = $account->status == 1 ? 'status_activated' : 'status_blocked';
    $mail_result = _user_mail_notify($op, $account);
    if ($mail_result == NULL) {
      drupal_set_message(t('The user account has been created.'));
    }
    else {
      drupal_set_message(t('The user account has been created. An email has been sent to the user.'));
    }
    // Redirect to the privileges tab.
    drupal_goto('user-manager/' . $account->uid . '/privileges');
  }
  else {
    drupal_set_message(t('Account could not be created. Try again.', 'error'));
  }
}