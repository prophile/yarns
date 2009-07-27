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
if (count($profiles) == 1)
{
	echo "Usage: " . $argv[0] . " profile [profile]\n";
	echo "Built-in profiles:\n";
	$contents = scandir($profile_dir);
	foreach ($contents as $item)
	{
		$item = str_replace(".profile", "", $item);
		if ($item != 'default' && $item[0] != '.')
			echo "\t$item\n";
	}
	exit(0);
}

function add_profile ( &$profile, $name )
{
	global $profile_dir;
	if (file_exists($name))
		$profile->addConfig($name);
	elseif (file_exists($profile_dir . "/$name.profile"))
		$profile->addConfig($profile_dir . "/$name.profile");
	elseif (preg_match('/([a-zA-Z0-9_]+)=(.+)/', $name, $matches))
		$profile->addVariable($matches[1], $matches[2]);
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

echo "Built configuration from profiles: " . implode(', ', $profiles) . "\n";

