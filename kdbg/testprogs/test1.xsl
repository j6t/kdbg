<?xml version="1.0" ?>
<!-- 
     File : test1.xsl     
     Author: Keith Isdale <k_isdale@tpg.com.au>
     Description: stylesheet for test "test1"
     Copyright Reserved Under GPL     
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

  <xsl:template match="/">
        <xsl:apply-templates/>
        <xsl:apply-templates mode="testMode"/>
	<xsl:apply-templates mode="xsl:testMode"/>
	<xsl:call-template name="test1"/>
	<xsl:call-template name="test2"/>
	<xsl:call-template name="xsl:test1"/>
	<xsl:call-template name="xsl:test2"/>
  </xsl:template>

  <xsl:template match="html">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="head">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="body">
    <xsl:apply-templates/>
  </xsl:template> 

  <xsl:template match="h1">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="para">
    <xsl:apply-templates/>
  </xsl:template>


  <xsl:template match="head" mode="testMode">
    <xsl:apply-templates mode="testMode"/>
  </xsl:template>

  <xsl:template match="body" mode="testMode">
    <xsl:apply-templates mode="testMode"/>
  </xsl:template> 

  <xsl:template match="h1" mode="testMode">
    <xsl:apply-templates mode="testMode"/>
  </xsl:template>

  <xsl:template match="para" mode="testMode">
    <xsl:apply-templates mode="testMode"/>
  </xsl:template>

  <xsl:template match="html" mode="testMode">
    <xsl:apply-templates mode="testMode"/>
  </xsl:template>


  <xsl:template match="head" mode="xsl:testMode">
    <xsl:apply-templates mode="xsl:testMode"/>
  </xsl:template>

  <xsl:template match="body" mode="xsl:testMode">
    <xsl:apply-templates mode="xsl:testMode"/>
  </xsl:template> 

  <xsl:template match="h1" mode="xsl:testMode">
    <xsl:value-of select="'test2'"/>
    <xsl:apply-templates select="."/>
    <xsl:apply-templates mode="xsl:testMode"/>
  </xsl:template>

  <xsl:template match="para" mode="xsl:testMode">
    <xsl:apply-templates mode="xsl:testMode"/>
  </xsl:template>

  <xsl:template match="html" mode="xsl:testMode">
    <xsl:apply-templates mode="xsl:testMode"/>
  </xsl:template>

  <xsl:template name="test1">
    <outputtest/>
  </xsl:template>

  <xsl:template name="test2">
    <outputtest/>
  </xsl:template>

  <xsl:template name="xsl:test1">
    <outputtest/>
  </xsl:template>

  <xsl:template name="xsl:test2">
    <outputtest/>
  </xsl:template>

  <xsl:template match="*">
    <outputtest/>
  </xsl:template>

  <xsl:template match="*" mode="testMode">
    <outputtest/>
  </xsl:template>

  <xsl:template match="*" mode="xsl:testMode">
    <outputtest/>
  </xsl:template>

</xsl:stylesheet>
