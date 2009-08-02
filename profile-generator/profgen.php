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
	elseif (file_exists("$profile_dir/$name.profile"))
		$profile->addConfig("$profile_dir/$name.profile");
	elseif (preg_match('/([a-zA-Z0-9_]+)=(.+)/', $name, $matches))
		$profile->addVariable($matches[1], $matches[2]);
	else
	{
		$fname = trim(strtolower(`uname -s`)) . "-$name-" . trim(`uname -m`);
		if (file_exists("$profile_dir/$fname.profile"))
			$profile->addConfig("$profile_dir/$fname.profile");
		else
			echo "unknown profile: $name / $fname\n";
	}
}

function file_hash ( $file )
{
	if (file_exists($file))
		return md5_file($file);
	else
		return '';
}

function writeout ( $data, $file )
{
	$md5d = md5($data);
	$md5f = file_hash($file);
	$bf = basename($file);
	if ($md5d == $md5f)
	{
		echo "$bf unchanged\n";
	}
	else
	{
		file_put_contents($file, $data);
		echo "wrote new $bf\n";
	}
}

$config = new profile($template_dir . '/config.h');
foreach ($profiles as $prof)
	add_profile($config, $prof);
writeout($config->generate(), "$base_dir/include/yarns/config.h");

$makefile = new profile($template_dir . '/Makefile');
foreach ($profiles as $prof)
	add_profile($makefile, $prof);
writeout($makefile->generate(), "$base_dir/Makefile");

echo "Built configuration from profiles: " . implode(', ', $profiles) . "\n";

