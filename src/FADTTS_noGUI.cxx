#include "FADTTS_noGUI.h"

//#include <QDebug>


/***************************************************************/
/********************** Public functions ***********************/
/***************************************************************/
FADTTS_noGUI::FADTTS_noGUI( QObject *parent ) :
    QObject( parent )
{
    m_matlabThread = new MatlabThread();
    connect( m_matlabThread, SIGNAL( WrongMatlabVersion() ), this, SLOT( OnUsingWrongMatlabVersion() ) );
    connect( m_matlabThread, SIGNAL( finished() ), this, SLOT( OnMatlabThreadFinished() ) );

    m_log = new Log();
    m_log->SetMatlabScript( m_matlabThread );

    connect( QThread::currentThread(), SIGNAL( finished() ), this, SLOT( OnKillFADTTSter() ) );

    InitFADTTS_noGUI();
}


int FADTTS_noGUI::RunFADTTSter_noGUI( const QJsonObject& jsonObject_noGUI )
{
    QJsonObject inputFiles = jsonObject_noGUI.value( "inputFiles" ).toObject();
    GetInputFiles( inputFiles );

    QJsonObject covariates = jsonObject_noGUI.value( "covariates" ).toObject();
    GetCovariates( covariates );

    QJsonObject subjects = jsonObject_noGUI.value( "subjects" ).toObject();
    GetSubjects( subjects );

    QJsonObject settings = jsonObject_noGUI.value( "settings" ).toObject();
    GetSettings( settings );

    QJsonObject matlabSpecifications = jsonObject_noGUI.value( "matlabSpecifications" ).toObject();
    GetMatlabSpecifications( matlabSpecifications );

    GetOutput( jsonObject_noGUI.value( "outputDir" ).toString() );

    if( CanFADTTSterBeRun() )
    {
        QJsonObject profile = jsonObject_noGUI.value( "profile" ).toObject();;

        SetMatlabScript( profile );

        m_matlabThread->start();
        while( m_matlabThread->isRunning() )
        {
            QThread::currentThread()->sleep( 1 );
        }

        m_log->AddText( "\nFile generation completed...\n" );

        return EXIT_SUCCESS;
    }
    else
    {
        return EXIT_FAILURE;
    }
}



/****************************************************************/
/**********************   Private slots   ***********************/
/****************************************************************/
void FADTTS_noGUI::OnUsingWrongMatlabVersion()
{
    QString warningMessage = "\nMaltab script will not be run.\n"
            "Due to compatibility issue, Matlab R2013b or more recent version is required.\n";
    m_log->AddText( tr( qPrintable( warningMessage ) ) );
    std::cout << warningMessage.toStdString().c_str() << std::endl;
}

void FADTTS_noGUI::OnMatlabThreadFinished()
{
    m_log->CloseLogFile();
}

void FADTTS_noGUI::OnKillFADTTSter()
{
    if( m_matlabThread->isRunning() )
    {
        m_log->AddText( "\nWarning! Thread terminated by user before completed matlab script.\n" );
        m_matlabThread->terminate();
        OnMatlabThreadFinished();
    }
}



/****************************************************************/
/********************** Private functions ***********************/
/****************************************************************/
void FADTTS_noGUI::InitFADTTS_noGUI()
{
    /*** Inputs ***/
    m_inputs.clear();
    m_properties.clear();
    m_covariates.clear();
    m_subjectColumnID = -1;

    /*** Subjects ***/
    m_subjectFile.clear();
    m_loadedSubjects.clear();

    m_subjects.clear();
    m_failedQCThresholdSubjects.clear();
    m_nbrSelectedSubjects = -1;
    m_qcThreshold = -1;

    /*** Settings ***/
    m_fibername.clear();
    m_nbrPermutations = -1;
    m_confidenceBandThreshold = -1;
    m_pvalueThreshold = -1;
    m_omnibus = false;
    m_posthoc = false;

    /*** Output ***/
    m_outputDir.clear();

    /*** Matlab Specifications ***/
    m_mvmcDir.clear();
    m_runMatlab = false;
    m_matlabExe.clear();
}


void FADTTS_noGUI::GetInputFiles( const QJsonObject& inputFiles )
{
    foreach( QString key, inputFiles.keys() )
    {
        QJsonObject currentPropertyObject = inputFiles.value( key ).toObject();
        QString currentPath = currentPropertyObject.value( "path" ).toString();
        if( !currentPath.isEmpty() && QFile( currentPath ).exists() )
        {
            int currentIndex = currentPropertyObject.value( "index" ).toInt();

            m_properties.insert( currentIndex, key );
            m_inputs.insert( currentIndex, currentPath );

            if( key == "SUBMATRIX" )
            {
                m_subjectColumnID = currentPropertyObject.value( "subjectColumnID" ).toInt();
            }
        }
    }
}

void FADTTS_noGUI::GetCovariates( const QJsonObject& covariates )
{
    QList< QStringList > subMatrixFile = m_processing.GetDataFromFile( m_inputs.value( SubMatrix ) );
    QStringList expectedCovariates;
    if( !subMatrixFile.isEmpty() )
    {
        expectedCovariates = subMatrixFile.first();
        expectedCovariates.removeAt( m_subjectColumnID );
        expectedCovariates.append( "Intercept" );
        expectedCovariates.sort();
    }

    QStringList covariatesFound = covariates.keys();

    if( expectedCovariates != covariatesFound )
    {
        std::cout << "/!\\ Covariates provided mismatched the ones found in the submatrix file provided." << std::endl;
        if( subMatrixFile.isEmpty() )
        {
            std::cout << "     --> no data extracted from subMatrix file" << std::endl;
        }
        else
        {
            if( expectedCovariates.size() != covariatesFound.size() )
            {
                std::cout << "     --> number of covariates is different" << std::endl;
            }
            else
            {
                std::cout << "     --> at least one covariate name provided mismatches" << std::endl << std::endl;
            }
        }
    }
    else
    {
        foreach( QString covariate, covariatesFound )
        {
            QJsonObject currentCovariateObject = covariates.value( covariate ).toObject();
            if( currentCovariateObject.value( "selected" ).toBool() )
            {
                m_covariates.insert( currentCovariateObject.value( "index" ).toInt(), covariate );
            }
        }
    }
}

QMap< int, QStringList > FADTTS_noGUI::GetInputSubjects()
{
    QMap< int, QStringList > subjectMap;

    QMap< int, QString >::ConstIterator iterInput = m_inputs.cbegin();
    while( iterInput != m_inputs.cend() )
    {
        QList< QStringList > fileData = m_processing.GetDataFromFile( iterInput.value() );
        subjectMap.insert( iterInput.key(), m_processing.GetSubjectsFromData( fileData, m_subjectColumnID ) );
        ++iterInput;
    }

    return subjectMap;
}

void FADTTS_noGUI::NANSubjects( QStringList allSubjects )
{
    QList< QStringList > faData = m_processing.GetDataFromFile( m_inputs.value( FA ) );
    if( !faData.isEmpty() )
    {
        QStringList nanSubjects = m_processing.GetNANSubjects( faData, allSubjects );
        if( !nanSubjects.isEmpty() )
        {
            std::cout << "/!\\ WARNING /!\\ subject(s) with -nan and/or nan values (FA file):" << std::endl;
            for( int i = 0; i < nanSubjects.size(); i++ )
            {
                std::cout << "    - " << nanSubjects.at( i ).toStdString() << std::endl;
            }
            std::cout << std::endl;
        }
    }
}

void FADTTS_noGUI::SetQCThreshold( const QJsonObject& qcThresholdObject )
{
    if( qcThresholdObject.value( "apply" ).toBool() )
    {
        double qcThreshold = qcThresholdObject.value( "value" ).toDouble();
        QList< QStringList > rawData = m_processing.GetDataFromFile( m_inputs.value( m_properties.key( "FA" ) ) );

        if( ( qcThreshold >= 0 ) && ( qcThreshold <= 1 ) && !rawData.isEmpty() )
        {
            bool useAtlas = qcThresholdObject.value( "useAtlas" ).toBool();
            m_qcThreshold = qcThreshold;
            m_processing.ApplyQCThreshold_noGUI( rawData, useAtlas, m_subjects, m_failedQCThresholdSubjects, m_qcThreshold );
        }
        else
        {
            std::cout << "... QC threshold not applied" << std::endl;
            if( !( ( qcThreshold >= 0 ) && ( qcThreshold <= 1 ) ) )
            {
                std::cout << "    - qc threshold provided must be 0 <= .. <= 1" << std::endl;
            }
            if( rawData.isEmpty() )
            {
                std::cout << "    - no file found to base qt threshold on" << std::endl;
            }
            std::cout << std::endl;
        }
    }
}

void FADTTS_noGUI::GetSubjects( const QJsonObject& subjects )
{
    m_subjectFile = subjects.value( "subjectListPath" ).toString();
    if( !m_subjectFile.isEmpty() )
    {
        m_loadedSubjects = m_processing.GetSubjectsFromFileList( m_subjectFile );
    }

    QMap< int, QStringList > allSubjects = GetInputSubjects();
    if( !m_loadedSubjects.isEmpty() )
    {
        allSubjects.insert( -1, m_loadedSubjects );
    }

    QStringList allSubjectsList = m_processing.GetAllSubjects( allSubjects );
    QMap< QString, QMap< int, bool > > sortedSubjects = m_processing.SortSubjects( allSubjectsList, allSubjects );

    QStringList matchedSubjects;
    QMap< QString, QList< int > > unMatchedSubjects;
    m_processing.AssignSortedSubject( sortedSubjects, matchedSubjects, unMatchedSubjects );
    m_subjects = matchedSubjects;

    NANSubjects( matchedSubjects );

    SetQCThreshold( subjects.value( "qcThreshold" ).toObject() );

    m_nbrSelectedSubjects = m_subjects.size();
}

void FADTTS_noGUI::GetSettings( const QJsonObject& settings )
{
    m_fibername = settings.value( "fiberName" ).toString();
    m_nbrPermutations = settings.value( "nbrPermutations" ).toInt();
    m_confidenceBandThreshold = settings.value( "confidenceBandThreshold" ).toDouble();
    m_pvalueThreshold = settings.value( "pvalueThreshold" ).toDouble();
    m_omnibus = settings.value( "omnibus" ).toBool();
    m_posthoc = settings.value( "posthoc" ).toBool();
}

void FADTTS_noGUI::GetMatlabSpecifications( const QJsonObject& matlabSpecifications )
{
    m_mvmcDir = matlabSpecifications.value( "fadttsDir" ).toString();

    m_matlabExe = matlabSpecifications.value( "matlabExe" ).toString();
    m_matlabThread->SetMatlabExe() = m_matlabExe;

    m_runMatlab = matlabSpecifications.value( "runMatlab" ).toBool();
    m_matlabThread->SetRunMatlab() = m_runMatlab;
}

void FADTTS_noGUI::GetOutput( QString outputDir )
{
    m_outputDir = outputDir + "/FADTTSter_" + m_fibername;
}



bool FADTTS_noGUI::CanFADTTSterBeRun()
{
    /*** Inputs ***/
    bool atLeastOneDiffusionPropertyFileIsProvided = m_properties.size() > 1;
    bool subMatrixFileIsProvided = m_properties.contains( 4 );
    bool atLeastOneCovariateIsProvided = m_covariates.size() > 0;
    bool subjectColumnIDIsProvided = m_subjectColumnID >= 0;

    /*** Subjects ***/
    bool subjectsAreProvided = !m_subjects.isEmpty();

    /*** Settings ***/
    bool fiberNameIsProvided = !m_fibername.isEmpty();
    bool nbrPermutationsIsProvided = m_nbrPermutations > 0;
    bool confidenceBandThresholdIsProvided = ( m_confidenceBandThreshold >= 0 ) && ( m_confidenceBandThreshold <= 1 );
    bool pvalueThresholdIsProvided = ( m_pvalueThreshold >= 0 ) && ( m_pvalueThreshold <= 1 );

    /*** Output ***/
    bool outputDirIsProvided = !m_outputDir.isEmpty() && QDir( QDir( m_outputDir ).currentPath() ).exists();

    /*** Matlab Specifications ***/
    bool mvcmPathIsProvided = !m_mvmcDir.isEmpty() && QDir( m_mvmcDir ).exists();
    bool matlabExeIsProvided = m_runMatlab ? ( !m_matlabExe.isEmpty() && QFile( m_matlabExe ).exists() ) : true;

    bool matlabCanBeRun = atLeastOneDiffusionPropertyFileIsProvided && subMatrixFileIsProvided &&
            atLeastOneCovariateIsProvided && subjectColumnIDIsProvided && subjectsAreProvided &&
            fiberNameIsProvided && nbrPermutationsIsProvided && confidenceBandThresholdIsProvided &&
            pvalueThresholdIsProvided && outputDirIsProvided && mvcmPathIsProvided
            && matlabExeIsProvided;

    if( !matlabCanBeRun )
    {
        std::cout << std::endl << "FADTTSter --noGUI will not be run for the following reason(s):" << std::endl;

        if( !atLeastOneDiffusionPropertyFileIsProvided )
        {
            std::cout << "/!\\ no diffusion property input file found. Check path and file name of each file provided." << std::endl;
        }
        if( !subMatrixFileIsProvided )
        {
            std::cout << "/!\\ covariate file not found. Check path and file name of the file provided." << std::endl;
        }
        if( !atLeastOneCovariateIsProvided )
        {
            std::cout << "/!\\ no covariate found. Check name, index and selection status of each covariate provided." << std::endl
                      << "     For each covariate, name and index must match the ones in the covariate file. ( reminder: 1st column = index 0 )" << std::endl;
        }
        if( !subjectColumnIDIsProvided )
        {
            std::cout << "/!\\ subjectColumnID not set. ( must be >= 0 )" << std::endl;
        }
        if( !subjectsAreProvided )
        {
            std::cout << "/!\\ no subject matched found to run FADTTSter." << std::endl;
        }
        if( !fiberNameIsProvided )
        {
            std::cout << "/!\\ fiber name not provided." << std::endl;
        }
        if( !nbrPermutationsIsProvided )
        {
            std::cout << "/!\\ nbrPermutations not set. ( must be >= 0 ) Usually set as 100 for tests, 1000 otherwise." << std::endl;
        }
        if( !confidenceBandThresholdIsProvided )
        {
            std::cout << "/!\\ confidenceBandThreshold not set. ( must be 0 <= . <= 1 )" << std::endl;
        }
        if( !pvalueThresholdIsProvided )
        {
            std::cout << "/!\\ pvalueThreshold not set. ( must be 0 <= . <= 1 )" << std::endl;
        }
        if( !outputDirIsProvided )
        {
            std::cout << "/!\\ output directory not provided." << std::endl;
        }
        if( !mvcmPathIsProvided )
        {
            std::cout << "/!\\ FADTTS directory not provided." << std::endl;
        }
        if( !matlabExeIsProvided )
        {
            std::cout << "/!\\ matlab executable not provided." << std::endl;
        }

        std::cout << std::endl;
    }

    return matlabCanBeRun;
}

void FADTTS_noGUI::GenerateSubjectFile()
{
    /** ****** Subject List ****** **/
    QFile selectedSubjectFile( m_outputDir + "/" + m_fibername + "_subjectList.txt" );
    if( selectedSubjectFile.open( QIODevice::WriteOnly ) )
    {
        QTextStream tsSelectedSubjectFile( &selectedSubjectFile );
        for( int i = 0; i < m_subjects.size(); i++ )
        {
            tsSelectedSubjectFile << m_subjects.at( i ) << endl;
        }
        selectedSubjectFile.flush();
        selectedSubjectFile.close();
    }

    /** ****** Failed qcThreshold Subject List ****** **/
    QFile failedQCThresholdSubjectFile( m_outputDir + "/" + m_fibername + "_failed_QCThreshold_SubjectList.txt" );
    if( failedQCThresholdSubjectFile.open( QIODevice::WriteOnly ) )
    {
        QTextStream tsFailedQCThresholdSubjectFile( &failedQCThresholdSubjectFile );

        tsFailedQCThresholdSubjectFile << "After failing the QC Threshold (" << QString::number( m_qcThreshold ) << ") the following subjects were removed from the study:" << endl;

        for( int i = 0; i < m_failedQCThresholdSubjects.size(); i++ )
        {
            tsFailedQCThresholdSubjectFile << "- " << m_failedQCThresholdSubjects.at( i ) << endl;
        }
        failedQCThresholdSubjectFile.flush();
        failedQCThresholdSubjectFile.close();
    }
}

void FADTTS_noGUI::SetMatlabScript( const QJsonObject& profile )
{
    QDir().mkpath( m_outputDir );

    GenerateSubjectFile();

    int startProfile = -1;
    int endProfile = -1;
    QList< QStringList > faData = m_processing.GetDataFromFile( m_inputs.value( FA ) );
    if( !faData.isEmpty() )
    {
        QStringList arcLength = m_processing.Transpose( faData ).first();
        arcLength.removeFirst();

        if( !arcLength.isEmpty() )
        {
            QString tempStartProfile = profile.value( "startProfile" ).toString();
            QString tempEndProfile = profile.value( "endProfile" ).toString();

            startProfile = arcLength.contains( tempStartProfile ) ? arcLength.indexOf( tempStartProfile, 0 ) : -1;
            endProfile = arcLength.contains( tempEndProfile ) ? arcLength.indexOf( tempEndProfile, 0 ) : -1;

            if( startProfile >= endProfile )
            {
                startProfile = -1;
                endProfile = -1;
            }
        }
    }


    QMap< int, QString > matlabInputFiles = m_processing.GenerateMatlabInputs( m_outputDir, m_fibername, m_inputs, m_properties, m_covariates,
                                                                               m_subjectColumnID, m_subjects, startProfile, endProfile );

    m_matlabThread->InitMatlabScript( m_outputDir, "FADTTSterAnalysis_" + m_fibername + "_" + QString::number( m_nbrPermutations ) + "perm.m" );
    m_matlabThread->SetHeader();
    m_matlabThread->SetMVCMPath( m_mvmcDir );
    m_matlabThread->SetFiberName( m_fibername );
    m_matlabThread->SetDiffusionProperties( m_properties.values() );
    m_matlabThread->SetNbrPermutation( m_nbrPermutations );
    m_matlabThread->SetCovariates( m_covariates );
    m_matlabThread->SetInputFiles( matlabInputFiles );
    m_matlabThread->SetOmnibus( m_omnibus );
    m_matlabThread->SetPostHoc( m_posthoc );
    m_matlabThread->SetConfidenceBandsThreshold( m_confidenceBandThreshold );
    m_matlabThread->SetPvalueThreshold( m_pvalueThreshold );

    m_log->SetLogFile( m_outputDir, m_fibername );
    m_log->InitLog( m_outputDir, m_fibername, matlabInputFiles, m_covariates, m_loadedSubjects, m_subjectFile, m_nbrSelectedSubjects,
                    m_failedQCThresholdSubjects, m_qcThreshold, m_nbrPermutations, m_confidenceBandThreshold, m_pvalueThreshold,
                    m_omnibus, m_posthoc, m_mvmcDir, m_runMatlab, m_matlabExe );
}
