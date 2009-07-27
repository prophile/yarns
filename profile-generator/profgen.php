#!/usr/bin/env php
<?php

$path = dirname(__FILE__);
$base_dir = dirname($path);
$profile_dir = $path . '/profiles';
$template_dir = $path . '/templates';

require('profile-generator.php');

$profiles = $argv;
array_shift($profiles);
array_unshift($profiles, 'default');

function add_profile ( &$profile, $name )
{
	global $profile_dir;
	if (file_exists($name))
		$profile->addConfig($name);
	elseif (file_exists($profile_dir . "/$name.profile"))
		$profile->addConfig($profile_dir . "/$name.profile");
	else
		echo "unknown profile: $name\n";
}

$config = new profile($template_dir . '/config.h');
foreach ($profiles as $prof)
	add_profile($config, $prof);
file_put_contents("$base_dir/lib/config.h", $config->generate());

$makefile = new profile($template_dir . '/Makefile');
foreach ($profiles as $prof)
	add_profile($makefile, $prof);
file_put_contents("$base_dir/Makefile", $makefile->generate());

