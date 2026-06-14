#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "cpu.cpp"
#include "disk.cpp"
#include "gpu.cpp"
#include "net.cpp"
#include "ops.cpp"
#include "procs.cpp"
#include "ram.cpp"

#define PROCESSES_NO_MAIN
#define CPU_NO_MAIN
#define RAM_NO_MAIN
#define DISK_NO_MAIN
#define GPU_NO_MAIN
#define NET_NO_MAIN

class ui : public QWidget {
public:
  ui() {
    setWindowTitle("System Monitor");
    resize(800, 600);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    auto *perftab = new QWidget(this);
    auto *perflayout = new QVBoxLayout(perftab);
    perflayout->setSpacing(10);
    perflayout->setContentsMargins(10, 10, 10, 10);

    auto *cpuGroup = new QGroupBox("Processor (CPU)", perftab);
    auto *cpuVBox = new QVBoxLayout(cpuGroup);
    cpuLabel = new QLabel("Usage: 0.0%", cpuGroup);
    cpuBar = new QProgressBar(cpuGroup);
    cpuBar->setRange(0, 100);
    cpuDetailsLabel = new QLabel("Loading...", cpuGroup);
    cpuVBox->addWidget(cpuLabel);
    cpuVBox->addWidget(cpuBar);
    cpuVBox->addWidget(cpuDetailsLabel);
    perflayout->addWidget(cpuGroup);

    auto *ramGroup = new QGroupBox("Memory (RAM)", perftab);
    auto *ramVBox = new QVBoxLayout(ramGroup);
    ramLabel = new QLabel("Usage: 0.0%", ramGroup);
    ramBar = new QProgressBar(ramGroup);
    ramBar->setRange(0, 100);
    ramDetailsLabel = new QLabel("Loading...", ramGroup);
    ramVBox->addWidget(ramLabel);
    ramVBox->addWidget(ramBar);
    ramVBox->addWidget(ramDetailsLabel);
    perflayout->addWidget(ramGroup);

    auto *diskGroup = new QGroupBox("Storage (Disk)", perftab);
    auto *diskVBox = new QVBoxLayout(diskGroup);
    diskDetailsLabel = new QLabel("Loading...", diskGroup);
    diskVBox->addWidget(diskDetailsLabel);
    perflayout->addWidget(diskGroup);

    auto *gpuGroup = new QGroupBox("Graphics (GPU)", perftab);
    auto *gpuVBox = new QVBoxLayout(gpuGroup);
    gpuLabel = new QLabel("GPU Info:", gpuGroup);
    gpuDetailsLabel = new QLabel("Loading...", gpuGroup);
    gpuVBox->addWidget(gpuLabel);
    gpuVBox->addWidget(gpuDetailsLabel);
    perflayout->addWidget(gpuGroup);

    tabs->addTab(perftab, "Performance");

    auto *procTab = new QWidget(this);
    auto *procLayout = new QVBoxLayout(procTab);
    procLayout->setContentsMargins(10, 10, 10, 10);

    procTable = new QTableWidget(this);
    procTable->setColumnCount(8);
    procTable->setHorizontalHeaderLabels(
        {"PID", "USER", "NI", "CPU%", "MEM%", "VIRT", "RES", "COMMAND"});

    procTable->horizontalHeader()->setSectionsClickable(true);
    procTable->horizontalHeader()->setSortIndicator(currentSortColumn,
                                                    currentSortOrder);
    procTable->horizontalHeader()->setSortIndicatorShown(true);
    connect(procTable->horizontalHeader(), &QHeaderView::sectionClicked, this,
            &ui::onHeaderClicked);

    procTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    procTable->horizontalHeader()->setSectionResizeMode(
        7, QHeaderView::ResizeToContents);
    procTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    procTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    procLayout->addWidget(procTable);

    auto *btnLayout = new QHBoxLayout();
    auto *niceBtn = new QPushButton("Set Priority (Nice)", this);
    auto *killBtn = new QPushButton("Kill Process", this);
    btnLayout->addWidget(niceBtn);
    btnLayout->addWidget(killBtn);
    btnLayout->addStretch();
    procLayout->addLayout(btnLayout);
    connect(niceBtn, &QPushButton::clicked, this, &ui::onNiceClicked);
    connect(killBtn, &QPushButton::clicked, this, &ui::onKillClicked);
    tabs->addTab(procTab, "Processes");

    cpuDummy = cpuMon.getRawStats();
    procMon.fetch();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ui::updateStats);
    timer->start(1000);
    updatestats();
  }
  QString formatUptime(long seconds) {
    long h = seconds / 3600;
    long m = (seconds % 3600) / 60;
    long s = seconds % 60;
    return QString("%1h %2m %3s").arg(h).arg(m).arg(s);
  }
  void onHeaderClicked(int logicalIndex) {
    if (currentSortColumn == logicalIndex) {
      currentSortOrder = (currentSortOrder == Qt::AscendingOrder)
                             ? Qt::DescendingOrder
                             : Qt::AscendingOrder;
    } else {
      currentSortColumn = logicalIndex;
      if (logicalIndex == 0 || logicalIndex == 1 || logicalIndex == 7) {
        currentSortOrder = Qt::AscendingOrder;
      } else {
        currentSortOrder = Qt::DescendingOrder;
      }
    }
    procTable->horizontalHeader()->setSortIndicator(currentSortColumn,
                                                    currentSortOrder);
    updatestats();
  }
  void updatestats() {
    cpuMon.fetch(cpu, cpuDummy);
    ramMon.fetch(ram);
    diskMon.fetch(disk);
    gpuMon.fetch(gpu);
    netMon.fetch(netIfaces);

    cpuBar->setValue((int)cpu.utilization);
    cpuLabel->setText(QString("Usage: %1%").arg(cpu.utilization, 0, 'f', 1));
    cpuDetailsLabel->setText(QString("Model: %1\n"
                                     "Cores: %2 Physical | %3 Logical\n"
                                     "Speed: %4 MHz\n"
                                     "L3 Cache: %5\n"
                                     "Uptime: %6\n"
                                     "Processes Created: %7")
                                 .arg(QString::fromStdString(cpu.model))
                                 .arg(cpu.cores)
                                 .arg(cpu.logical)
                                 .arg(cpu.currentSpeed, 0, 'f', 0)
                                 .arg(QString::fromStdString(cpu.l3Cache))
                                 .arg(formatUptime(cpu.uptime))
                                 .arg(cpu.processes));

    ramBar->setValue((int)ram.utilization);
    ramLabel->setText(QString("Usage: %1%").arg(ram.utilization, 0, 'f', 1));
    ramDetailsLabel->setText(
        QString("Total RAM: %1 GB\n"
                "In Use: %2 GB | Available: %3 GB\n"
                "Swap Used: %4 GB / %5 GB\n"
                "Buffers: %6 MB | Cached: %7 MB\n"
                "Active: %8 MB | Inactive: %9 MB")
            .arg(ram.total / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(ram.used / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(ram.avail / (1024.0 * 1024.0), 0, 'f', 2)
            .arg((ram.stotal - ram.sfree) / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(ram.stotal / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(ram.buffers / 1024.0, 0, 'f', 1)
            .arg(ram.cached / 1024.0, 0, 'f', 1)
            .arg(ram.active / 1024.0, 0, 'f', 1)
            .arg(ram.inactive / 1024.0, 0, 'f', 1));

    diskDetailsLabel->setText(
        QString("Device Path: %1\n"
                "Total Storage: %2 GB\n"
                "Used: %3 GB (%4%) | Free: %5 GB\n"
                "Active Time: %6%\n"
                "Read Speed: %7 MB/s | Write Speed: %8 MB/s")
            .arg(QString::fromStdString(disk.devicePath))
            .arg(disk.totalB / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2)
            .arg(disk.usedB / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2)
            .arg(disk.capacityUtil, 0, 'f', 1)
            .arg(disk.freeB / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2)
            .arg(disk.activeVal, 0, 'f', 1)
            .arg(disk.readMBS, 0, 'f', 2)
            .arg(disk.writeMBS, 0, 'f', 2));

    if (gpu.hasGPU) {
      gpuLabel->setText(
          QString("GPU Usage: %1%").arg(gpu.utilization, 0, 'f', 1));
      gpuDetailsLabel->setText(QString("Model: %1\n"
                                       "Driver: %2\n"
                                       "Temp: %3°C | VRAM: %4 GB / %5 GB")
                                   .arg(QString::fromStdString(gpu.gpuldvendor))
                                   .arg(QString::fromStdString(gpu.driver))
                                   .arg(gpu.tempC, 0, 'f', 0)
                                   .arg(gpu.vramUsed, 0, 'f', 2)
                                   .arg(gpu.vramTotal, 0, 'f', 2));
    } else {
      gpuLabel->setText("GPU Usage: N/A");
      gpuDetailsLabel->setText("No GPU detected on this system.");
    }

    double totalRx = 0, totalTx = 0;
    for (const auto &iface : netIfaces) {
      totalRx += iface.rxSpeedMbps;
      totalTx += iface.txSpeedMbps;
    }
    netLabel->setText(
        QString("Total Speed: Down: %1 | Up: %2")
            .arg(QString::fromStdString(netMonitor::formatSpeed(totalRx)))
            .arg(QString::fromStdString(netMonitor::formatSpeed(totalTx))));

    QString netInfo;
    for (const auto &iface : netIfaces) {
      netInfo +=
          QString(
              "Interface %1: Down: %2 | Up: %3 (Total RX: %4 GB | TX: %5 GB)\n"
              "  IPv4: %6 | IPv6: %7\n")
              .arg(QString::fromStdString(iface.name))
              .arg(QString::fromStdString(
                  netMonitor::formatSpeed(iface.rxSpeedMbps)))
              .arg(QString::fromStdString(
                  netMonitor::formatSpeed(iface.txSpeedMbps)))
              .arg(iface.totalRxBytes)
              .arg(iface.totalTxBytes)
              .arg(QString::fromStdString(iface.ipv4))
              .arg(QString::fromStdString(iface.ipv6));
    }
    netDetailsLabel->setText(netInfo.trimmed());

    std::vector<ProcessInfo> procs = procMon.fetchProcesses();

    int selectedPid = -1;
    int currentRow = procTable->currentRow();
    if (currentRow >= 0 && procTable->item(currentRow, 0)) {
      selectedPid = procTable->item(currentRow, 0)->text().toInt();
    }

    std::sort(procs.begin(), procs.end(),
              [this](const ProcessInfo &a, const ProcessInfo &b) {
                bool asc = (currentSortOrder == Qt::AscendingOrder);
                switch (currentSortColumn) {
                case 0:
                  return asc ? (a.pid < b.pid) : (a.pid > b.pid);
                case 1:
                  return asc ? (a.user < b.user) : (a.user > b.user);
                case 2:
                  return asc ? (a.nice < b.nice) : (a.nice > b.nice);
                case 3:
                  return asc ? (a.cpuPercent < b.cpuPercent)
                             : (a.cpuPercent > b.cpuPercent);
                case 4:
                  return asc ? (a.memPercent < b.memPercent)
                             : (a.memPercent > b.memPercent);
                case 5:
                  return asc ? (a.virtMb < b.virtMb) : (a.virtMb > b.virtMb);
                case 6:
                  return asc ? (a.resMb < b.resMb) : (a.resMb > b.resMb);
                case 7:
                  return asc ? (a.command < b.command)
                             : (a.command > b.command);
                default:
                  return false;
                }
              });
    procTable->setRowCount(0);
    int count = 0;
    for (const auto &p : procs) {
      if (count >= 30)
        break;
      int row = procTable->rowCount();
      procTable->insertRow(row);

      procTable->setItem(row, 0, new QTableWidgetItem(QString::number(p.pid)));
      procTable->setItem(row, 1,
                         new QTableWidgetItem(QString::fromStdString(p.user)));
      procTable->setItem(row, 2, new QTableWidgetItem(QString::number(p.nice)));
      procTable->setItem(
          row, 3,
          new QTableWidgetItem(QString("%1%").arg(p.cpuPercent, 0, 'f', 1)));
      procTable->setItem(
          row, 4,
          new QTableWidgetItem(QString("%1%").arg(p.memPercent, 0, 'f', 1)));
      procTable->setItem(
          row, 5,
          new QTableWidgetItem(QString::fromStdString(formatMemory(p.virtMb))));
      procTable->setItem(
          row, 6,
          new QTableWidgetItem(QString::fromStdString(formatMemory(p.resMb))));

      string cmd = p.command;
      if (cmd.length() > 65)
        cmd = cmd.substr(0, 62) + "...";
      procTable->setItem(row, 7,
                         new QTableWidgetItem(QString::fromStdString(cmd)));

      count++;
    }

    if (selectedPid != -1) {
      for (int r = 0; r < procTable->rowCount(); ++r) {
        if (procTable->item(r, 0)->text().toInt() == selectedPid) {
          procTable->selectRow(r);
          break;
        }
      }
    }
  }

private slots:
  void onKillClicked() {
    int row = procTable->currentRow();
    if (row < 0) {
      QMessageBox::warning(this, "Warning",
                           "Please select a process from the list first.");
      return;
    }
    int pid = procTable->item(row, 0)->text().toInt();
    QString cmd = procTable->item(row, 7)->text();

    auto btn = QMessageBox::question(
        this, "Kill Process",
        QString("Are you sure you want to terminate PID %1 (%2)?")
            .arg(pid)
            .arg(cmd),
        QMessageBox::Yes | QMessageBox::No);

    if (btn == QMessageBox::Yes) {
      if (killProcess(pid)) {
        QMessageBox::information(
            this, "Success",
            QString("Process %1 (PID: %2) was terminated.").arg(cmd).arg(pid));
      } else {
        QMessageBox::critical(this, "Failed",
                              "You might not have sufficient permissions.");
      }
    }
  }

  void onNiceClicked() {
    int row = procTable->currentRow();
    if (row < 0) {
      QMessageBox::warning(this, "Warning",
                           "Please select a process from the list first.");
      return;
    }
    int pid = procTable->item(row, 0)->text().toInt();
    int currentNice = procTable->item(row, 2)->text().toInt();

    bool ok;
    int newNice = QInputDialog::getInt(
        this, "Set Process Priority (Nice)",
        QString("Enter new Nice value for PID %1 (-20 to 19)",
                "Lower values is higher priority.")
            .arg(pid),
        currentNice, -20, 19, 1, &ok);

    if (ok) {
      if (setProcessPriority(pid, newNice)) {
        QMessageBox::information(this, "Success",
                                 QString("Priority for PID %1 was set to %2.")
                                     .arg(pid)
                                     .arg(newNice));
      } else {
        QMessageBox::critical(this, "Failed",
                              "Setting negative nice values requires admin.");
      }
    }
  }

private:
  cpuMonitor cpuMon;
  ramMonitor ramMon;
  diskMonitor diskMon;
  gpuMonitor gpuMon;
  netMonitor netMon;
  ProcessMonitor procMon;

  cpuData cpu;
  cpuStats cpuDummy;
  ramData ram;
  diskData disk;
  gpuData gpu;
  std::vector<netData> netIfaces;

  QLabel *cpuLabel;
  QProgressBar *cpuBar;
  QLabel *cpuDetailsLabel;
  QLabel *ramLabel;
  QProgressBar *ramBar;
  QLabel *ramDetailsLabel;
  QLabel *diskDetailsLabel;
  QLabel *gpuLabel;
  QLabel *gpuDetailsLabel;
  QLabel *netLabel;
  QLabel *netDetailsLabel;
  QTableWidget *procTable;

  int currentSortColumn = 3;
  Qt::SortOrder currentSortOrder = Qt::DescendingOrder;
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  ui ui;
  ui.show();
  return app.exec();
}
