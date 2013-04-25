<?php

/**
 * @file
 * Theming functions for the User Manager.
 */

/**
 * Renders the list of users for the User Manager, with the edit links and the
 * "Add new user" link.
 *
 * @ingroup themeable
 */
function theme_user_manager_userlist($variables) {
  global $base_url;

  $output = '';
  $accounts = $variables['accounts'];

  if (empty($accounts)) {
    $output = 'There are no users you can manage.';
  }
  else {
    $output .= '<div id="user-manager-userlist">';
    $output .= '<div class="slidesorter-user-manager">';
    foreach ($accounts as $uid => $account) {
      $user_edit_link = $base_url . '/user-manager/' . $account->uid . '/edit';
      $output .= '<div class="user-row">';

      $output .= '<span class="slidesorter-user-manager-username">' . $account->name . '</span>';
      $output .= '<div class="btn-right"><a href="' . $user_edit_link . '" class="slidesorter-btn">Edit</a></div>';

      $output .= '</div>';
    }
    $output .= '</div>';
    $output .= '</div>';
  }

  $output .= '<div class="user-manager-add-user">
                <a href="'. $base_url .'/user-manager/add" class="slidesorter-btn">Add new user</a>
                </div>';

  return $output;
}
