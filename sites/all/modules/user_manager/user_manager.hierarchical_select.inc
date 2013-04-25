<?php

/**
 * @file
 * Page callback for the privileges page of the User Manager. This file also
 * contains all the code relating the usage of the Hierarchical Select API.
 */

define('USER_MANAGER_HS_ROLE_ADMIN', 'admin');
define('USER_MANAGER_HS_ROLE_MANAGER', 'manager');
define('USER_MANAGER_HS_ROLE_USER', 'user');

/**
 * Concatenates all information and returns output of privilege form
 */
function user_manager_privileges($account) {
  // Table with the priviles of the user.
  $roles_table = user_manager_privileges_table($account);

  // Hides input form if the user is Content Administrator
  if(!in_array(SLIDESORTER_ROLES_CONTENT_ADMINISTRATOR, $account->roles)) {
    // Hierarchical_select form.
    $hs_form = drupal_render(drupal_get_form('user_manager_hs_form', $account));
  }

  $output = $roles_table . '<div class="clearfix"></div>' . $hs_form;
  return $output;
}


/**
 * Checks user privileges and returns them as an array of rows for the table
 */
function user_manager_privileges_table($account) {
  // Get the user privileges array.
  $user_privileges = user_manager_get_user_privileges($account);
  $rows = array();

  // Checks if user has global 'Content Administrator' role, as this is the only one that needs to be shown if true
  if (in_array(SLIDESORTER_ROLES_CONTENT_ADMINISTRATOR, $account->roles)) {
    $privilege_type = USER_MANAGER_HS_ROLE_ADMIN;
    $role = 'Administrator';
    $client = 'All';

    $rows[] = array(
      $role,
      $client,
      drupal_render(drupal_get_form('user_privilege_form', $privilege_type, $group_node, $account)),
    );
  } else {
    // The privilege type is fetched from the node content type.
    foreach ($user_privileges as $gid => $group_node) {

      $user_roles_in_group = og_get_user_roles('node', $gid, $account->uid);

      // Admin privilege.
      if (in_array('group manager', $user_roles_in_group)) {
        $privilege_type = USER_MANAGER_HS_ROLE_MANAGER;
        $role = 'Manager';
        $client = $group_node->title;
        //$product = ' - ';
      }

      // Viewer privilege.

      else if (in_array('member', $user_roles_in_group)) {
        $privilege_type = USER_MANAGER_HS_ROLE_USER;
        $role = 'User';
        //$client_parent = node_load($group_node->og_group_ref[LANGUAGE_NONE][0]['target_id']);
        $client = $group_node->title;
        //$product = $group_node->title;
      }

      // No product or client. This node should be here....
      else {
        continue;
      }

      // Populate the row that will be passed to the table.
      $rows[] = array(
        $role,
        $client,
        //$product, AM - Column not required
        drupal_render(drupal_get_form('user_privilege_form', $privilege_type, $group_node, $account)),
      );
    }
  }

  $variables = array(
    'header' => array('Role', 'Product', ''),
    'rows' => $rows,
    'empty' => t('No privileges available for this user'),
  );
  $output = theme('table', $variables);

  return $output;
}

function user_privilege_form($form, &$form_state) {
  // Privilege type.
  $privilege_type = $form_state['build_info']['args'][0];
  // Node (group) for which the privilege is given.
  $node_group = $form_state['build_info']['args'][1];

  $form['privilege']['type'] = array(
    '#type' => 'hidden',
    '#value' => $privilege_type,
  );
  $form['privilege']['gid'] = array(
    '#type' => 'hidden',
    '#value' => $node_group->nid,
  );
  $form['privilege']['remove'] = array(
    '#type' => 'submit',
    '#value' => t('Delete'),
  );

  return $form;
}

function user_privilege_form_submit($form, &$form_state) {
  // Node (group) for which the privilege is given.
  $account = $form_state['build_info']['args'][2];

  // Using what comes in $form_state['values'] returns always the values of the
  // first item in the list, because all buttons trigger the form with the values
  // on the first element. Ideally, find a way around that, but using 'input' instead now.
  $privilege_type = $form_state['input']['type'];
  $group_id = $form_state['input']['gid'];
  $group_node = node_load($group_id);

//  dpm($form_state);
//  dpm($privilege_type);
//  dpm($group_id);
//  dpm($group_node);

  // TODO: Very important. If it's a "product manager" membership, need to check
  // if there's a Client Manager membership for this user, for the client of the
  // product. If that's the case, this user is both administrator and group manager,
  // so only the manager role should be revoked, but the full membership shouldn't be removed.
  // In practice, this should never happen, but it might be the case if a manager
  // adds a product manager role to a user, and then for the same client ads a client manager
  // role. The product manager role could be removed afterwards, and that would destroy the
  // whole membership, but that shouldn't happen. For consistency, perhaps a better solution
  // would be to remove all memberships to the subgroups of a Client when a "Client Manager" role
  // is granted, and then add the Client Manager membership. This would ensure the proper
  // permissions are in place, and would avoid "duplication", as it doesn't really make sense
  // for an administrator of a group, to be a product manager of the same group. Leaving this
  // here just in case changed are done, but it has been fixed by rebuilding the whole
  // group hierarchy for the user when granting him a Client Manager role.


  // If the privilege type is set to admin, which is only the case for the role of Content Administrator,
  // this role will be removed rather than attempting to remove og memberships
  if ($privilege_type == USER_MANAGER_HS_ROLE_ADMIN) {
    $user = user_load($account->uid);
    if (in_array(SLIDESORTER_ROLES_CONTENT_ADMINISTRATOR, $user->roles)) {
      $content_manager_global_role = array_search(SLIDESORTER_ROLES_CONTENT_ADMINISTRATOR, $user->roles);
      unset($user->roles[$content_manager_global_role]);
      user_save($user);
    }
  } else {
    // All the info is available. Remove the role and membership for the affected
    // User - group.
    $membership = og_get_membership('node', $group_id, 'user', $account->uid);

    try {
      og_membership_delete($membership->id);
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }
}


function user_manager_hs_form($form, &$form_state) {
  $super_administrator = user_manager_current_user_administrator();

  $form['user_manager_hs']['role'] = array(
    '#type' => 'select',
    '#title' => t('Role'),
    '#options' => user_manager_get_assignable_roles($super_administrator),
  );
  // Hierarchical select section.
  $hierarchy = user_manager_get_privileges_hierarchy();

  $form['user_manager_hs']['client_product'] = array(
    '#type' => 'hierarchical_select',
    '#title' => t('Product'),
    '#size' => 1,
    '#config' => array(
      'module' => 'hs_smallhierarchy',
      'params' => array(
        'hierarchy' => $hierarchy,
        'id' => 'user_manager_hierarchy',
        'separator' => '|',
      ),
      'save_lineage'    => 0,
      'enforce_deepest' => (int) !$super_administrator,
      'entity_count'    => 0,
      'require_entity'    => 0,
      'resizable'       => 0,
      'level_labels' => array(
        'status' => 1,
        'labels' => array(
          0 => t(' - '),
          1 => t(' - '),
        ),
      ),
      'dropbox' => array(
        'status'   => 0,
        'title'    => t('All selections'),
        'limit'    => 0,
        'reset_hs' => 1,
      ),
      'editability' => array(
        'status'           => 0,
        'item_types'       => array(),
        'allowed_levels'   => array(
          0 => 0,
          1 => 0,
        ),
        'allow_new_levels' => 0,
        'max_levels'       => 2,
      ),
      // These settings cannot be configured through the UI: they can only be
      // overridden through code.
      'animation_delay'    => 400,
      'exclusive_lineages' => array(),
      'render_flat_select' => 0,
    ),
    '#description' => '',
    '#default_value' => '',
    '#required' => TRUE,
  );
  $form['user_manager_hs']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add role'),
  );
  $form['user_manager_hs']['cancel_button'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#element_validate' => array('user_manager_return_dashboard'),
  );

  return $form;
}

/**
 * Validate handler for the User Manager Privileges section form.
 */
function user_manager_hs_form_validate($form, &$form_state) {

  $is_super_administrator = user_manager_current_user_administrator();
  $hs_values = $form_state['input']['client_product']['hierarchical_select']['selects'];
  $role = $form_state['input']['role'];

  // At least something from the first level must be chosen, if they have not picked administrator
  if ($hs_values[0] == 'label_0' && $role != USER_MANAGER_HS_ROLE_ADMIN) {
    form_set_error('client_product', t('You must choose a Product from the list.'));
  }

  // Checks that they pick the null value if they are adding administrator privilege
  if ($hs_values[0] != 'label_0' && $role == USER_MANAGER_HS_ROLE_ADMIN) {
    form_set_error('client_product', t('Administrator privilege should be added to all products (-).'));
  }

  // For administrators, if the Role chosen is Product Manager, they must have chosen a product.
  /* AM - Removed as there is no second level dropdown

  else if ($is_super_administrator && $form_state['values']['role'] == USER_MANAGER_HS_ROLE_USER && $hs_values[1] == 'label_1') {
    form_set_error('client_product', t('You must choose a Product in order to grant the Product Manager role.'));
  }
  // For administrators, if the Role chosen is Client Manager, they can't choose a product.
  else if ($is_super_administrator && $form_state['values']['role'] == USER_MANAGER_HS_ROLE_MANAGER && isset($hs_values[1]) && $hs_values[1] != 'label_1') {
    form_set_error('client_product', t('You can\'t assign the Client Manager role to a Product.'));
  }

  */

//  dpm($hs_values);
}

/**
 * Submit handler for the User Manager Privileges section form.
 */
function user_manager_hs_form_submit($form, &$form_state) {
  $account = $form_state['build_info']['args'][0];
  $hs_values = $form_state['input']['client_product']['hierarchical_select']['selects'];
  $role = $form_state['values']['role'];
  $group_gid = $hs_values[0];

  // If the role to be added is an administrator
  if ($role == USER_MANAGER_HS_ROLE_ADMIN) {
    
    // If a User has been granted the Administrator role,
    // grant them the global Content Administrator role
    $user = user_load($account->uid);
    if (!in_array($role, $account->roles)) {
      $content_admin_role = user_role_load_by_name(SLIDESORTER_ROLES_CONTENT_ADMINISTRATOR);
      $user->roles[$content_admin_role->rid] = $content_admin_role->name;
      user_save($user);
    } else {
      drupal_set_message(t('This user is already an administrator.'), 'error');
    }

    /* AM - Removed

    $gnode = node_load($group_gid);
    $role_granted = user_manager_og_load_role_by_name(SLIDESORTER_GROUP_ROLES_MANAGER, $gnode->type);
    if ($gnode->type == SLIDESORTER_CLIENT_CONTENT_TYPE && $role_granted->name == SLIDESORTER_GROUP_ROLES_MANAGER) {
      $user = user_load($account->uid);
      if (!in_array(SLIDESORTER_ROLES_CLIENT_MANAGER, $user->roles)) {
        $client_manager_global_role = user_role_load_by_name(SLIDESORTER_ROLES_CLIENT_MANAGER);
        $user->roles[$client_manager_global_role->rid] = $client_manager_global_role->name;
        user_save($user);
      }
    }

    */

    // If the role selected is Client Manager, remove all memberships of this
    // user for the selected group. This is done for consistency, and rebuilds
    // the Roles hierarchy for the user within the Client Group selected.
    
    /* AM - Removed as there are no sub-groups

    $subgroups = user_manager_get_group_subgroups($group_gid);

    foreach ($subgroups as $subgroup) {
      $subgroup_membership = og_get_membership('node', $subgroup->nid, 'user', $account->uid);
      if ($subgroup_membership) {
        og_membership_delete($subgroup_membership->id);
      }
    }

  */

  } else {
    /* AM - Removed as it might be problematic

    else if ($role == USER_MANAGER_HS_ROLE_USER) {
      $client_product = explode('|', $hs_values[1]);
      $group_gid = $client_product[1];
      dpm($group_id);
    }

    */

    // Need this to load the role name.
    $group_node = node_load($group_gid);
  //  dpm($group_node);
  //  dpm($account);
    // Add the user to the desired group, if he's not already in it.
    if (!og_get_membership('node', $group_gid, 'user', $account->uid)) {
      try {
        $membership = og_membership_create('node', $group_gid, 'user', $account->uid, 'og_user_node');
  //      dpm($membership);
        $save_membership = og_membership_save($membership);
  //      dpm($membership);

        if ($role == USER_MANAGER_HS_ROLE_MANAGER) {
          $selected_role = user_manager_og_load_role_by_name(SLIDESORTER_GROUP_ROLES_MANAGER, $group_node->type);
  //      dpm($selected_role);
          og_role_grant('node', $group_gid, $account->uid, $selected_role->rid);
        }
  //      dpm($group_gid);
  //      dpm($account);

        // Final step. If the Role is CLIENT_MANAGER, assign to the user the global Client Manager role.
        // Removed from here. This is done in hook_og_role_grant().

        // TODO: Before printing this, the $save_membership value should be fixed.
        // Issue is that OGroups doesn't return nothing for that function (even though
        // the documentation says it does). OG Needs a patch for that.
        drupal_set_message(t('User Privileges updated successfully.'));
      }
      catch (Exception $e) {
        drupal_set_message(t($e->getMessage()), 'error');
      }
    }
    else {
      drupal_set_message(t('That membership already exists.'), 'error');
    }
  }
}

/**
 * Example of a hierarchy:
 *
 * $hierarchy = array(
 *   'win' => array(
 *     'label' => 'Windows',
 *     'children' => array(
 *       'xp'    => array('label' => 'XP'),
 *       'vista' => array(
 *         'label' => 'Vista',
 *         'children' => array(
 *           'x86' => array('label' => '32-bits'),
 *           'x64' => array('label' => '64-bits'),
 *         ),
 *       ),
 *     ),
 *   ),
 * );
 *
 * @return array
 */
function user_manager_get_privileges_hierarchy() {
  $hierarchy = array();

  // First, get Client groups for the current user.
  $user_managed_clients_nodes = node_load_multiple(array_keys(user_manager_get_user_client_nodes()));

  // Removes Webform Default node
  foreach ($user_managed_clients_nodes as $nid => $node) {
    if($node->title == 'Webform Default Fields Master Node') {
      unset($user_managed_clients_nodes[$nid]);
    }    
  }

  // Iterate over each client.
  foreach ($user_managed_clients_nodes as $nid => $client_node) {
    $hierarchy[$nid] = array(
      'label' => $client_node->title,
      //'children' => array(),
    );

    /* AM - Removed children call

    // Get the products of the client.
    $current_client_products = user_manager_og_membership_multiple(array($nid), 'node');
    // Assign them to the hierarchy.
    foreach ($current_client_products as $membership_id => $membership_meta) {
      $membership = og_membership_load($membership_id);
      $product_node = node_load($membership->etid);

      // Make sure we're only adding product nodes!
      if ($product_node->type == SLIDESORTER_PRODUCT_CONTENT_TYPE) {
        $hierarchy[$nid]['children'][$product_node->nid] = array(
          'label'  => $product_node->title,
        );
      }
    }

    */

  }
  //dpm($hierarchy);

  return $hierarchy;
}

function user_manager_get_assignable_roles($super_administrator = TRUE) {
  if ($super_administrator) {
    $assignable_roles = array(
      USER_MANAGER_HS_ROLE_ADMIN => 'Administrator',
      USER_MANAGER_HS_ROLE_MANAGER => 'Manager',
      USER_MANAGER_HS_ROLE_USER => 'User',
    );
  }
  else {
    $assignable_roles = array(
      USER_MANAGER_HS_ROLE_USER => 'User',
    );
  }

  return $assignable_roles;
}

/**
 * @param $account
 *  A user account object.
 *
 * return Array
 *  An array with all the groups for which the user specified has the
 *  Group Manager role.
 */

function user_manager_get_user_privileges($account) {
  $user_privileges = array();
  $user_group_ids = og_get_groups_by_user($account, 'node');
  $user_group_nodes = node_load_multiple($user_group_ids);
//  dpm($user_group_nodes);
  // Use this array to prevent products from appearing in the table if the
  // user is already Client Manager for the Client that a product belongs to.
  // However, this shouldn't be needed, since if that's the case, the user
  // shouldn't be Group Manager for the product, but "administrator" instead.
  // TODO: Come back at this and check whether it should be used or not. It's not
  // used for now.
  $client_nodes_id = array();

  foreach ($user_group_nodes as $nid => $node) {
    $user_roles_in_group = og_get_user_roles('node', $nid, $account->uid);
    //dpm($user_roles_in_group);
    $user_privileges[$nid] = $node;
    
    /* AM - Removed to show all group members

    $is_manager = in_array(SLIDESORTER_GROUP_ROLES_MANAGER, $user_roles_in_group);

    if ($is_manager) {
      $user_privileges[$nid] = $node;
    }
    // Any other groups, or groups for which the user is not manager, are not desired.
    // Remove from the array in case we need to reuse later. TODO: Remove if
    // not used in the end.
    
    else {
      unset($user_group_nodes[$nid]);
    }

    */

  }
  //dpm($user_privileges);
  return $user_privileges;
}
