<?php

class profile
{
	private $variables = array();
	private $data = '';
	
	function __construct ( $profile )
	{
		$this->data = file_get_contents($profile);
	}
	
	function addVariable ( $var, $value )
	{
		$this->variables[$var] = $value;
	}
	
	function addConfig ( $configFile )
	{
		$config = parse_ini_file($configFile);
		foreach ($config as $key => $value)
		{
			$this->addVariable($key, $value);
		}
	}
	
	function _callback_var ( $matches )
	{
		if (!empty($this->variables[$matches[1]]))
		{
			return $this->variables[$matches[1]];
		}
		else
		{
			return '';
		}
	}
	
	function _callback_if ( $matches )
	{
		if (!(empty($this->variables[$matches[1]]) || $this->variables[$matches[1]] == 0))
		{
			return $matches[2];
		}
		else
		{
			return '';
		}
	}
	
	function _callback_ifnot ( $matches )
	{
		if (empty($this->variables[$matches[1]]) || $this->variables[$matches[1]] == 0)
		{
			return $matches[2];
		}
		else
		{
			return '';
		}
	}
	
	function generate ()
	{
		$this->data = preg_replace_callback('/\{\$([a-zA-Z0-9_]+)\}/U', array($this, '_callback_var'), $this->data);
		$this->data = preg_replace_callback('/\{if \$([a-zA-Z0-9_]+)\}((.|\n)+)\{\/if\}/U', array($this, '_callback_if'), $this->data);
		$this->data = preg_replace_callback('/\{ifnot \$([a-zA-Z0-9_]+)\}((.|\n)+)\{\/ifnot\}/U', array($this, '_callback_ifnot'), $this->data);
		return $this->data;
	}
};

?>
