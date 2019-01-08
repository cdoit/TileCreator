#include "tilecreator.h"
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtCore/QDirIterator>

#include <osg/io_utils>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>

#include <osgEarth/Common>
#include <osgEarth/Map>
#include <osgEarth/MapNode>
#include <osgEarth/Registry>
#include <osgEarth/StringUtils>
#include <osgEarth/HTTPClient>
#include <osgEarth/TileVisitor>
#include <osgEarth/ImageLayer>
#include <osgEarth/ElevationLayer>
#include <osgEarthUtil/TMSPackager>
#include <osgEarthDrivers/feature_ogr/OGRFeatureOptions>
#include <osgEarthDrivers/tms/TMSOptions>
#include <osgEarthDrivers/gdal/GDALOptions>
#include <iostream>
#include <sstream>
#include <iterator>
#include<QtCore/QProcess>
#include <QtWidgets/QApplication>
using namespace osgEarth;
using namespace osgEarth::Util;
using namespace osgEarth::Drivers;
TileCreator::TileCreator(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	init();
	connect(ui.ptnres, SIGNAL(clicked()), this, SLOT(slotopenfile()));
	connect(ui.ptnobj, SIGNAL(clicked()), this, SLOT(slotsavefile()));
	connect(ui.ptnbegin, SIGNAL(clicked()), this, SLOT(slotbeginceator()));
	connect(ui.cmbTileCreator, SIGNAL(currentIndexChanged(int)), ui.cmbext, SLOT(setCurrentIndex(int)));
}

TileCreator::~TileCreator()
{

}

void TileCreator::addSubFolderFileImages(const QString &path, QStringList &list)
{
	QDir dir(path);
	dir.setFilter(QDir::Files | QDir::NoSymLinks);
	QFileInfoList list1 = dir.entryInfoList();
	int filecount = list1.count();
	if (filecount = 0)
	{
		return;
	}
	for (int i = 0; i < filecount; i++)
	{
		QFileInfo infor = list1.at(i);
		QString suffix = infor.suffix();
		if (QString::compare(suffix, QString("tif"), Qt::CaseInsensitive) == 0)
		{
			QString absolutefile_path = infor.absoluteFilePath();
			list.append(absolutefile_path);
		}
	}
}

void TileCreator::slotopenfile()
{
	QString filter("Images (*.png *.tif *.tiff *.jpg) /n Shp(*.shp)");
	QStringList filters;
	filters << QString("*.jpeg") << QString("*.jpg") << QString("*.png") << QString("*.tif") << QString("*.tiff") << QString("*.shp");
	QStringList shpfilters;
	shpfilters << QString("*.shp");
	QFileDialog* fd = new QFileDialog(this);//�����Ի���
	fd->resize(240, 320);    //������ʾ�Ĵ�С
	fd->setNameFilter(filter); //�����ļ�������
	fd->setViewMode(QFileDialog::List);  //�������ģʽ���� �б�list�� ģʽ�� ��ϸ��Ϣ��detail�����ַ�ʽ
	if (fd->exec() == QDialog::Accepted)   //����ɹ���ִ��
	{

		QStringList list;
		list = fd->selectedFiles();      //�����ļ��б������
		resFile = list.at(0).toLocal8Bit().constData();          //ȡ��һ���ļ���
		ui.ledresfile->setText(resFile);
#if 0 //ÿ��Ӱ���ڵ������ļ���

		if (ui.cmbTileflag->currentIndex() == 1)
		{
			int mark = resFile.lastIndexOf("/");
			resFile = resFile.left(mark);
			mark = resFile.lastIndexOf("/");
			resFile = resFile.left(mark);
			QDirIterator  dir_iterator(resFile, filters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
			while (dir_iterator.hasNext())
			{
				dir_iterator.next();
				QFileInfo fileifor = dir_iterator.fileInfo();
				QString absolute_file_path = fileifor.absoluteFilePath();
				fileNameList.append(absolute_file_path);
			}
		}
#elif 1//����Ӱ����ͬһ���ļ���
		if (ui.cmbTileflag->currentIndex() == 1)
		{

			int mark1 = resFile.lastIndexOf("/");
			resFile = resFile.left(mark1);
			QDir dir(resFile);
			//�����ļ�
			QFileInfoList file_list;
			if (ui.cmbTileCreator->currentIndex() == 2)
			{
				file_list = dir.entryInfoList(shpfilters, QDir::Files | QDir::Hidden | QDir::NoSymLinks);
			}
			else
			{
				file_list = dir.entryInfoList(filters, QDir::Files | QDir::Hidden | QDir::NoSymLinks);
			}
			//�����ļ���
			//QFileInfoList folder_list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

			for (int i = 0; i != file_list.size(); i++)
			{
				QString name = file_list.at(i).absoluteFilePath();
				fileNameList.append(name);
			}
		}
#endif

		//�����ʸ��,���׺��Ϊ��json
		if (ui.cmbTileCreator->currentIndex() == 2)
		{
			std::string extension = "json";
			ui.cmbext->setCurrentIndex(2);
		}
	}
	else
	{
		fd->close();
	}

}

void TileCreator::slotsavefile()
{
	objFile = QFileDialog::getExistingDirectory(this, QStringLiteral("��ѡ����Ƭ����·��..."), "./");
	if (!objFile.isEmpty())
	{
		ui.ledobjfile->setText(objFile);
	}
}



bool TileCreator::init()
{
	ui.setupUi(this);
	setWindowTitle(QStringLiteral("��ͼ��Ƭ"));
	ui.menuBar->hide();
	ui.mainToolBar->hide();
	ui.statusBar->hide();
	resize(600, 500);
	setWindowIcon(QIcon(QPixmap(":/TileCreator/Resources/images/TileCreator.png")));
	ui.progressBar->hide();
	return true;
}

int TileCreator::makeTMS()
{
	osgDB::Registry::instance()->getReaderWriterForExtension("png");
	osgDB::Registry::instance()->getReaderWriterForExtension("jpg");
	osgDB::Registry::instance()->getReaderWriterForExtension("tiff");

	//Read the min level
	unsigned int minLevel = 0;

	//Read the max level
	unsigned int maxLevel;
	if (ui.ledMaxlev->text().size() == 0)
	{
		int button = QMessageBox::information(this, QStringLiteral("����"), QStringLiteral("��Ƭ�����δ����, ��Ĭ������Ϊ5���Ƿ������"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::No);
		if (button == 1024)
		{
			maxLevel = 5;
		}
		else
		{
			return 2;
		}

	}
	else
	{
		maxLevel = ui.ledMaxlev->text().toInt();
	}

	std::vector< Bounds > bounds;
	// restrict packaging to user-specified bounds.    
	std::string tileList;
	bool verbose = false;
	unsigned int batchSize = 0;
	// Read the concurrency level
	unsigned int concurrency = 0;

	concurrency = ui.spinBox->value();

	bool applyAlphaMask = false;

	bool writeXML = true;

	// load up the map
	osg::ref_ptr<Map>map = new Map;
	GDALOptions basemap;
	if (ui.cmbTileCreator->currentIndex() == 0)
	{
		if (ui.cmbTileflag->currentIndex() == 0)
		{
			basemap.url() = ui.ledresfile->text().toStdString();
			map->addLayer(new ImageLayer(ImageLayerOptions("Tmsout", basemap)));
		}
		else
		{
			for (int i = 0; i < fileNameList.count(); i++)
			{
				basemap.url() = fileNameList.at(i).toLocal8Bit().constData();
				map->addLayer(new ImageLayer(ImageLayerOptions("Tmsout", basemap)));
			}
		}
	}
	else
	{
		if (ui.cmbTileflag->currentIndex() == 0)
		{
			basemap.url() = ui.ledresfile->text().toStdString();
			map->addLayer(new ElevationLayer(ElevationLayerOptions("Tmsout", basemap)));
		}
		else
		{
			for (int i = 0; i < fileNameList.count(); i++)
			{
				basemap.url() = fileNameList.at(i).toLocal8Bit().constData();
				map->addLayer(new ElevationLayer(ElevationLayerOptions("Tmsout", basemap)));
			}
		}
	}

	osg::ref_ptr<MapNode> mapNode = new MapNode(map);

	std::string index;

	// see if the user wants to override the type extension (imagery only)
	std::string extension;
	if (ui.cmbTileCreator->currentIndex() == 0)
	{
		extension = "png";
	}
	else if (ui.cmbTileCreator->currentIndex() == 1)
	{
		extension = "tif";
	}
	// folder to which to write the TMS archive.
	std::string rootFolder = ui.ledobjfile->text().toStdString();

	// whether to overwrite existing tile files
	//TODO:  Support
	bool overwrite = false;

	// write out an earth file
	std::string outEarth;

	std::string dbOptions;
	std::string::size_type n = 0;
	while ((n = dbOptions.find('"', n)) != dbOptions.npos)
	{
		dbOptions.erase(n, 1);
	}

	osg::ref_ptr<osgDB::Options> options = new osgDB::Options(dbOptions);

	// whether to keep 'empty' tiles    
	bool keepEmpties = false;

	//TODO:  Single color
	bool continueSingleColor = false;

	// elevation pixel depth
	unsigned elevationPixelDepth = 32;

	// create a folder for the output
	osgDB::makeDirectory(rootFolder);
	if (!osgDB::fileExists(rootFolder))
	{
		QMessageBox::warning(this, QStringLiteral("����"), QStringLiteral("·�������ڣ�"));
		return 0;
	}
	int imageLayerIndex = -1;

	int elevationLayerIndex = -1;

	osg::ref_ptr< TileVisitor > visitor;


	// If we dont' have a visitor create one.
	if (!visitor.valid())
	{
		// Create a multithreaded visitor
		MultithreadedTileVisitor* v = new MultithreadedTileVisitor();
		if (concurrency > 0)
		{
			v->setNumThreads(concurrency);
		}
		visitor = v;
	}

	osg::ref_ptr< ProgressCallback > progress = new ConsoleProgressCallback();

	if (verbose)
	{
		visitor->setProgressCallback(progress);
	}

	visitor->setMinLevel(minLevel);
	visitor->setMaxLevel(maxLevel);


	//for (unsigned int i = 0; i < bounds.size(); i++)
	//{
	//	GeoExtent extent(mapNode->getMapSRS(), bounds[i]);
	//	OE_DEBUG << "Adding extent " << extent.toString() << std::endl;
	//	visitor->addExtent(extent);
	//}

	// Setup a TMSPackager with all the options.
	TMSPackager packager;
	packager.setExtension(extension);
	packager.setVisitor(visitor);
	packager.setDestination(rootFolder);
	packager.setElevationPixelDepth(elevationPixelDepth);
	packager.setWriteOptions(options);
	packager.setOverwrite(overwrite);
	packager.setKeepEmpties(keepEmpties);
	packager.setApplyAlphaMask(applyAlphaMask);


	// new map for an output earth file if necessary.
	osg::ref_ptr<Map> outMap = 0L;
	if (!outEarth.empty())
	{
		// copy the options from the source map first
		outMap = new Map(map->getInitialMapOptions());
	}

	std::string outEarthFile = osgDB::concatPaths(rootFolder, osgDB::getSimpleFileName(outEarth));


	// Package an individual image layer
	if (imageLayerIndex >= 0)
	{
		ImageLayer* layer = map->getLayerAt<ImageLayer>(imageLayerIndex);
		if (layer)
		{
			packager.run(layer, map);
			if (writeXML)
			{
				packager.writeXML(layer, map);
			}
		}
		else
		{
			QString index;
			index.setNum(imageLayerIndex);
			QMessageBox::warning(this, QStringLiteral("����"), "Failed to find an image layer at  " + index);
			return 1;
		}
	}
	// Package an individual elevation layer
	else if (elevationLayerIndex >= 0)
	{
		ElevationLayer* layer = map->getLayerAt<ElevationLayer>(elevationLayerIndex);
		if (layer)
		{
			packager.run(layer, map);
			if (writeXML)
			{
				packager.writeXML(layer, map);
			}
		}
		else
		{
			QString index;
			index.setNum(elevationLayerIndex);
			QMessageBox::warning(this, QStringLiteral("����"), "Failed to find an elevation layer at  " + index);
			return 1;
		}
	}
	else
	{
		ImageLayerVector imageLayers;
		map->getLayers(imageLayers);

		// Package all the ImageLayer's
		for (unsigned int i = 0; i < imageLayers.size(); i++)
		{
			ImageLayer* layer = imageLayers.at(i);
			OE_NOTICE << "Packaging " << layer->getName() << std::endl;
			osg::Timer_t start = osg::Timer::instance()->tick();
			packager.run(layer, map);
			osg::Timer_t end = osg::Timer::instance()->tick();
			if (verbose)
			{
				OE_NOTICE << "Completed seeding layer " << layer->getName() << " in " << prettyPrintTime(osg::Timer::instance()->delta_s(start, end)) << std::endl;
			}

			if (writeXML)
			{
				packager.writeXML(layer, map);
			}

			// save to the output map if requested:
			if (outMap.valid())
			{
				std::string layerFolder = toLegalFileName(packager.getLayerName());

				// new TMS driver info:
				TMSOptions tms;
				tms.url() = URI(
					osgDB::concatPaths(layerFolder, "tms.xml"),
					outEarthFile);

				ImageLayerOptions layerOptions(packager.getLayerName(), tms);
				layerOptions.mergeConfig(layer->options().getConfig());
				layerOptions.cachePolicy() = CachePolicy::NO_CACHE;

				outMap->addLayer(new ImageLayer(layerOptions));
			}
		}

		// Package all the ElevationLayer's
		ElevationLayerVector elevationLayers;
		map->getLayers(elevationLayers);

		for (unsigned int i = 0; i < elevationLayers.size(); i++)
		{
			ElevationLayer* layer = elevationLayers.at(i);
			OE_NOTICE << "Packaging " << layer->getName() << std::endl;
			osg::Timer_t start = osg::Timer::instance()->tick();
			packager.run(layer, map);
			osg::Timer_t end = osg::Timer::instance()->tick();
			if (verbose)
			{
				OE_NOTICE << "Completed seeding layer " << layer->getName() << " in " << prettyPrintTime(osg::Timer::instance()->delta_s(start, end)) << std::endl;
			}
			if (writeXML)
			{
				packager.writeXML(layer, map);
			}

			// save to the output map if requested:
			if (outMap.valid())
			{
				std::string layerFolder = toLegalFileName(packager.getLayerName());

				// new TMS driver info:
				TMSOptions tms;
				tms.url() = URI(
					osgDB::concatPaths(layerFolder, "tms.xml"),
					outEarthFile);

				ElevationLayerOptions layerOptions(packager.getLayerName(), tms);
				layerOptions.mergeConfig(layer->options().getConfig());
				layerOptions.cachePolicy() = CachePolicy::NO_CACHE;

				outMap->addLayer(new ElevationLayer(layerOptions));
			}
		}

	}
	return 0;
}

int TileCreator::makeTFS()
{
	//Read the min level
	unsigned int minLevel = 0;

	//Read the max level
	unsigned int maxLevel;
	if (ui.ledMaxlev->text().size() == 0)
	{
		int button = QMessageBox::information(this, QStringLiteral("����"), QStringLiteral("��Ƭ�����δ����, �����ݳ����Զ����㣬�Ƿ������"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::No);
		if (button == 1024)
		{
			maxLevel = 5;
		}
		else
		{
			return 2;
		}

	}
	else
	{
		maxLevel = ui.ledMaxlev->text().toInt();
	}



	// see if the user wants to override the type extension (imagery only)

	// folder to which to write the TMS archive.
	std::string rootFolder = ui.ledobjfile->text().toStdString();


	// create a folder for the output
	osgDB::makeDirectory(rootFolder);
	if (!osgDB::fileExists(rootFolder))
	{
		QMessageBox::warning(this, QStringLiteral("����"), QStringLiteral("·�������ڣ�"));
		return 1;
	}
	//�����ļ�
	int flag = -3;
	if (ui.cmbTileflag->currentIndex() == 0)
	{
#ifdef _DEBUG
		flag = QProcess::execute(QApplication::applicationDirPath() + "\\osgearth_tfsd", QStringList() << resFile << " "
			<< "--max-level" << QString::number(maxLevel)<< " "
			<< "--out" << objFile + "//TFS");
#else
		flag = QProcess::execute(QApplication::applicationDirPath() + "\\osgearth_tfs", QStringList() << resFile << " "
			<< "--max-level" << QString::number(maxLevel) << " "
			<< "--out" << objFile + "//TFS");
#endif 
	}
	else
	{
		for (int i = 0; i < fileNameList.count(); i++)
		{
#ifdef _DEBUG
			flag = QProcess::execute(QApplication::applicationDirPath() + "\\osgearth_tfsd", QStringList()  << fileNameList[i] << " "
				<< "--max-level" << QString::number(maxLevel)<< " "
				<< "--out" << objFile + "//TFS" + QString::number(i));
#else
			flag = QProcess::execute(QApplication::applicationDirPath() + "\\osgearth_tfs", QStringList() << fileNameList[i] << " "
				<< "--max-level" << QString::number(maxLevel) << " "
				<< "--out" << objFile + "//TFS" + QString::number(i));
#endif 
		}
	}
	if (flag == -1 || flag == -2)
	{
		return 1;
	}
	return 0;
}

void TileCreator::slotbeginceator()
{
	if (ui.ledresfile->text().size() > 0 && ui.ledobjfile->text().size() > 0)
	{
		int i = -3;
		if (ui.cmbTileCreator->currentIndex() == 2)
		{
			i = makeTFS();
		}
		else
		{
			i = makeTMS();
		}

		if (i == 0)
		{
			QMessageBox::information(this, QStringLiteral("֪ͨ"), QStringLiteral("��Ƭ��ɣ�"));
		}
		else if (i == 1)
		{
			QMessageBox::information(this, QStringLiteral("֪ͨ"), QStringLiteral("��Ƭʧ�ܣ�"));
		}
	}
	else if (ui.ledresfile->text().size() == 0)
	{
		QMessageBox::warning(this, QStringLiteral("����"), QStringLiteral("������ѡ��ԭʼ���ݣ�"));
	}
	else if (ui.ledobjfile->text().size() == 0)
	{
		QMessageBox::warning(this, QStringLiteral("����"), QStringLiteral("������ѡ����Ƭ�Ĵ洢·����"));
	}
}