#include "kscript_parser.h"
#include "kscript_context.h"
#include "kscript_func.h"
#include "kscript_class.h"
#include "kscript.h"
#include "kscript_qt.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#include <qfile.h>
#include <qtextstream.h>

#include <kglobal.h>
#include <kstddirs.h>

KSInterpreter::KSInterpreter()
{
    m_outStream = 0;
    m_currentArg = -1;
    m_outDevice = 0;
    m_lastInputLine = new KSValue( QString() );
    m_lastInputLine->setMode( KSValue::LeftExpr );

    KSModule::Ptr m = ksCreateModule_KScript( this );
    m_modules.insert( m->name(), m );

    // This module will serve as our global namespace
    // since adressing all kscript builtin stuff via its module
    // is too much typing for our users.
    m_global = m->nameSpace();

    m = ksCreateModule_Qt( this );
    m_modules.insert( m->name(), m );

    // Integrate the Qt module in the global namespace for convenience
    KSNamespace::Iterator it = m->nameSpace()->begin();
    KSNamespace::Iterator end = m->nameSpace()->end();
    for(; it != end; ++it )
	m_global->insert( it.key(), it.data() );

    m_globalContext.setScope( new KSScope( m_global, 0 ) );
}

KSInterpreter::~KSInterpreter()
{
    if ( m_outStream )
	delete m_outStream;
    if ( m_outDevice )
    {
	m_outDevice->close();
	delete m_outDevice;
    }
}

KSModule::Ptr KSInterpreter::module( const QString& name )
{
  QMap<QString,KSModule::Ptr>::Iterator it = m_modules.find( name );
  if ( it == m_modules.end() )
    return 0;

  return it.data();
}

QString KSInterpreter::runScript( const QString& filename, const QStringList& args )
{
    // Save for usage by the <> operator
    m_args = args;

    KSContext context( m_globalContext );
    // The "" indicates that this is not a module but
    // a script in its original meaning.
    if ( !runModule( context, "", filename, args ) )
	return context.exception()->toString( context );

    return QString::null;
}

bool KSInterpreter::runModule( KSContext& context, const QString& name )
{
  // Did we load this module already ? Dont load it twice
  if ( m_modules.contains( name ) )
  {
    KSModule* m = m_modules[name];
    m->ref();
    context.setValue( new KSValue( m ) );

    return true;
  }

  QString ksname = name + ".ks";
  QString qdlname = name + ".qdl";
  QString pname = name + ".la";

  QStringList::Iterator it = m_searchPaths.begin();
  for( ; it != m_searchPaths.end(); ++it )
  {
    DIR *dp = 0L;
    struct dirent *ep;

    dp = opendir( *it );
    if ( dp == 0L )
      return false;

    while ( ( ep = readdir( dp ) ) != 0L )
    {
      if ( ksname == ep->d_name || qdlname == ep->d_name || pname == ep->d_name )
      {
	QString f = *it;
	f += "/";
	f += ep->d_name;
	struct stat buff;
	if ( ( stat( f, &buff ) == 0 ) && S_ISREG( buff.st_mode ) )
	{
	  QStringList lst;
	  qDebug("RUN MODULE %s %s", name.latin1(), f.latin1() );
	  return runModule( context, name, f, lst );
	}
      }
    }

    closedir( dp );
  }

  // Search in /opt/kde/lib
  QString libfile = KGlobal::dirs()->findResource( "lib", pname );
  if ( !libfile.isEmpty() )
  {
      QStringList lst;
      qDebug("RUN PEBBLES MODULE %s %s", name.latin1(), libfile.latin1() );
      return runModule( context, name, libfile, lst );
  }

  QString tmp( "Could not find module %1" );
  context.setException( new KSException( "IOError", tmp.arg( name ) ) );
  return false;
}

bool KSInterpreter::runModule( KSContext& result, const QString& name, const QString& filename, const QStringList& args )
{
  // Did we load this module already ? Dont load it twice
  if ( m_modules.contains( name ) )
  {
    KSModule* m = m_modules[name];
    m->ref();
    result.setValue( new KSValue( m ) );

    return true;
  }

  m_globalContext.setException( 0 );

  // s_current = this;

  FILE* f = fopen( filename, "r" );
  if ( !f )
  {
    QString tmp( "Could not open file %1" );
    result.setException( new KSException( "IOError", tmp.arg( filename ) ) );
    return false;
  }

  KSModule::Ptr module;
  /* if ( filename.right(4) == ".qdl" )
  {
      QFile file( filename );
      file.open( IO_ReadOnly, f );

      // ### TODO: Check for parser errors
      QDomDocument doc( &file );

      // Create a module
      module = new KSWinModule( this, name, doc );
  }
  else if ( filename.right(3) == ".la" )
  {
      // Create a module
      module = new KSPebblesModule( this, name );
  }
  else */
  {
      // Create the parse tree.
      KSParser parser;
      if ( !parser.parse( f, filename.ascii() ) )
      {
	  fclose( f );
	  result.setException( new KSException( "SyntaxError", parser.errorMessage() ) );
	  return false;
      }
      // Create a module
      module = new KSModule( this, name, parser.donateParseTree() );
  }
  fclose( f );
  // parser.print( true );

  // Put the module in the return value
  module->ref();
  result.setValue( new KSValue( &*module ) );

  // Put all global functions etc. in the scope
  KSContext context;
  // ### TODO: Do we create a memory leak here ?
  context.setScope( new KSScope( m_global, module ) );

  // Travers the parse tree to find functions and classes etc.
  if ( !module->eval( context ) )
  {
    if ( context.exception() )
    {
      result.setException( context );
      return false;
    }
    // TODO: create exception
    printf("No exception available\n");
    return false;
  }

  // Is there a main function to execute ?
  KSValue* code = module->object( "main" );
  if ( code )
  {
    // create a context that holds the argument list
    // for the main function ( the list is empty currently ).
    KSContext context;
    context.setValue( new KSValue( KSValue::ListType ) );
    // ### TODO: Do we create a memory leak here ?
    context.setScope( new KSScope( m_global, module ) );

    // Insert parameters
    QStringList::ConstIterator sit = args.begin();
    QStringList::ConstIterator send = args.end();
    for( ; sit != send; ++sit )
    {
	context.value()->listValue().append( new KSValue( *sit ) );
    }

    if ( !code->functionValue()->call( context ) )
    {
      if ( context.exception() )
      {
	result.setException( context );
	return false;
      }

      // TODO: create exception
      printf("No exception available\n");
      return false;
    }
  }

  // Dump the namespace
  /* printf("\nNamespace\n---------\n");
  KSNamespace::Iterator it = m_space.begin();
  for( ; it != m_space.end(); ++it )
  {
    printf("%s = %s\n",it.key().ascii(),it.data()->toString().ascii() );
    if ( it.data()->type() == KSValue::ClassType )
    {
      KSNamespace::Iterator it2 = it.data()->classValue()->nameSpace()->begin();
      KSNamespace::Iterator end = it.data()->classValue()->nameSpace()->end();
      for( ; it2 != end; ++it2 )
	printf("  %s = %s\n",it2.key().ascii(),it2.data()->toString().ascii() );
    }
    } */

  // Clear the namespace
  // m_space.clear();
  // m_globalContext.setValue( 0 );

  /* Not needed any more, or ?
  if ( m_globalContext.exception() )
    m_globalContext.exception()->print();
  m_globalContext.setException( 0 );
  */

  KSException* ex = m_globalContext.shareException();
  m_globalContext.setException( 0 );
  if ( ex )
  {
    result.setException( ex );
    return false;
  }

  // Did we just execute a file ? -> Done
  if ( name.isEmpty() )
    return true;

  m_modules.insert( name, module );

  return true;
}

bool KSInterpreter::processExtension( KSContext& context, KSParseNode* node )
{
  QString tmp( "The interpreter does not support an extended syntax you are using.");
  context.setException( new KSException( "UnsupportedSyntaxExtension", tmp, node->getLineNo() ) );

  return false;
}

KRegExp* KSInterpreter::regexp()
{
    return &m_regexp;
}

QString KSInterpreter::readInput()
{
    if ( !m_outStream )
    {
	if ( m_args.count() > 0 )
        {
	    m_currentArg = 0;
	    m_outDevice = new QFile( m_args[ m_currentArg ] );
	    m_outDevice->open( IO_ReadOnly );
	    m_outStream = new QTextStream( m_outDevice );
	}
	else
	    m_outStream = new QTextStream( stdin, IO_ReadOnly );
    }

    QString tmp = m_outStream->readLine();

    if ( !tmp.isNull() )
    {
	tmp += "\n";
	m_lastInputLine->setValue( tmp );
	return tmp;
    }

    m_lastInputLine->setValue( tmp );
	
    // Ended reading a file ...

    // Did we scan the last file ?
    if ( m_currentArg == (int)m_args.count() - 1 )
	return QString();
    else
    {
	m_currentArg++;
	if ( m_outStream )
	    delete m_outStream;
	if ( m_outDevice )
	    delete m_outDevice;
	m_outDevice = new QFile( m_args[ m_currentArg ] );
	m_outDevice->open( IO_ReadOnly );
	m_outStream = new QTextStream( m_outDevice );
    }

    return readInput();
}

KSValue::Ptr KSInterpreter::lastInputLine() const
{
    return m_lastInputLine;;
}
