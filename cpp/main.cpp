#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include <sys/stat.h> // for mkdir()
#include <unistd.h>   // for access()

#include "path_planner.h"

#include "inih/cpp/INIReader.h"

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    printf("Path Planning for 2D map!\n"
           "\tUsage: %s <*.ini>\n",
           argv[0]);
    return -1;
  }
  // 检查是不是ini文件
  std::string cfgFile(argv[1]);
  if (cfgFile.substr(cfgFile.size() - 4) != ".ini") {
    fprintf(stderr, "%s is not a configuration file\n", argv[1]);
    return -1;
  }

  INIReader cfg(cfgFile);
  if (cfg.ParseError() < 0) {
    fprintf(stderr, "Can not parse configuration file: %s\n", argv[1]);
    return -1;
  }

  std::string inputFile = cfg.Get("IO", "input", "");
  GridMap gm;
  std::ifstream ifs(inputFile);
  bool shouldSolvePath = false;
  if (ifs.is_open()) {
    // 路径规划问题
    shouldSolvePath = true;
    std::string line;
    // 输入是只含 0，1 的 .csv，代表二值图
    std::vector<std::string> tmp;
    while (std::getline(ifs, line)) {
      tmp.push_back(line);
    }
    ifs.clear();
    ifs.close();

    if (tmp.size() == 0) {
      return 0;
    }
    int rowNum = tmp.size();
    int colNum = (tmp[0].size() + 1) / 2;
    gm.resize(rowNum, colNum);
    for (int i = 0; i < rowNum; i++) {
      for (int j = 0; j < colNum; j++) {
        int idx = i * colNum + j;
        GridCell *curCell = gm.getCellByRowCol(i, j);
        curCell->setIndex(idx);
        char c = tmp[i][2 * j];
        if (c == '0') {
          //障碍物
          curCell->setVelocity(0);
        }
      }
    }

    // 如果需要远离边界
    if (cfg.GetBoolean("Parameters", "awayFromBoundary", false) == true) {
      // 设置地图周围一圈为障碍物
      gm.setBoundary();
    }

    // 先设置边界再设置终点
    int endRow = cfg.GetInteger("Parameters", "endRow", -1);
    int endCol = cfg.GetInteger("Parameters", "endCol", -1);
    if (gm.setDestination(endRow, endCol) < 0) {
      fprintf(stderr, "Error: set destination at obstacle: (%d, %d),\n", endRow,
              endCol);
      return -1;
    }

  } else {
    #include <cmath>
    std::cerr << "Err: open file failed, solve eikonal equation with precision = "<< inputFile << "\n";
    // 求解数值方程
    // 精度
    double delta = cfg.GetReal("IO", "input", 0.01);
    // 方程
    auto v = [](double x, double y) -> double {
      double f = 2*M_PI*std::sqrt(std::cos(2*M_PI*x)*std::cos(2*M_PI*x)*std::sin(2*M_PI*y)*std::sin(2*M_PI*y) +
                              std::sin(2*M_PI*x)*std::sin(2*M_PI*x)*std::cos(2*M_PI*y)*std::cos(2*M_PI*y));
      return 1.0/f;
    };
    auto xlim = std::make_pair(0.0, 1.0);
    auto ylim = std::make_pair(0.0, 1.0);
    int rowNum = (xlim.second - xlim.first)/delta;
    int colNum = (ylim.second - ylim.first)/delta;
    gm.resize(rowNum, colNum, delta);
    for(int i=0; i<rowNum; i++) {
      for(int j = 0; j<colNum; j++) {
        int idx = i * colNum + j;
        GridCell *curCell = gm.getCellByRowCol(i, j);
        curCell->setIndex(idx);
        curCell->setVelocity(v(xlim.first + i*delta, ylim.first + j*delta));
      }
    }

    // 设置已知到达函数的点
    auto setMathCell = [&](double x, double y, double val) {
      int row = (x - xlim.first)/delta;
      int col = (y - ylim.first)/delta;
      gm.getCellByRowCol(row, col)->setArrivalTime(val);
    };
    setMathCell(0.25, 0.25, 1);
    setMathCell(0.75, 0.75, 1);
    setMathCell(0.25, 0.75, 1);
    setMathCell(0.75, 0.25, 1);
    setMathCell(0.5, 0.5, 2);
    // 左右边界
    for (int i=0; i<gm.rows(); i++) {
      gm.getCellByRowCol(i, 0)->setArrivalTime(0.0);
      gm.getCellByRowCol(i, gm.cols()-1)->setArrivalTime(0.0);
    }
    // 上下边界
    for (int j=0; j<gm.cols(); j++) {
      gm.getCellByRowCol(0, j)->setArrivalTime(0.0);
      gm.getCellByRowCol(gm.rows()-1, j)->setArrivalTime(0.0);
    }
  }

  // 计算时间矩阵
  int limCount = cfg.GetInteger("Parameters", "fsmSweepCountLim", 100);
  int thresh = cfg.GetInteger("Parameters", "parallelThresh", 1000);
  auto start = std::chrono::steady_clock::now();
  std::string method = cfg.Get("Parameters", "method", "");
  if (method == "fsm") {
    PathPlanner::fsm(gm, limCount);
  } else if (method == "pfsm") {
    PathPlanner::pfsm(gm, limCount);
  } else if (method == "parallel_fsm") {
    PathPlanner::parallel_fsm(gm, limCount, thresh);
  } else if (method == "sfsm") {
    PathPlanner::sfsm(gm, limCount);
  } else if (method == "fmm") {
    PathPlanner::fmm(gm);
  } else if (method == "sfmm") {
    PathPlanner::sfmm(gm);
  } else if (method == "afm2") {
    // 为了编程方便，此处终点与起点颠倒
    // 终点是船的位置，起点是目标点
    double heading = cfg.GetReal("Parameters", "heading", 0.0);
    double theta = cfg.GetReal("Parameters", "theta", 0.0);
    int radius = cfg.GetReal("Parameters", "radius", 0);
    PathPlanner::afm2(gm, heading, theta, radius);
  } else if (method == "my_afm2"){
    double heading = cfg.GetReal("Parameters", "heading", 0.0);
    int radius = cfg.GetReal("Parameters", "radius", 0);
    PathPlanner::my_afm2(gm, heading, radius);
  } else {
    std::cout << "Unkown method: " << method << std::endl;
    return -1;
  }
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> diff = end - start;
  std::cout << method << " cost " << diff.count()
            << " seconds " << std::endl;

  // 计算路径
  std::vector<std::pair<int, int>> path;
  if (shouldSolvePath == true) {
    int startRow = cfg.GetInteger("Parameters", "startRow", -1);
    int startCol = cfg.GetInteger("Parameters", "startCol", -1);
    int stepSize = cfg.GetInteger("Parameters", "stepSize", 1);
    path = PathPlanner::getPath(gm, startRow, startCol, stepSize);
  }


  // 保存结果
  bool shouldSaveResult = cfg.GetBoolean("IO", "saveResult", false);
  if (shouldSaveResult) {
    std::string outputDir = cfg.Get("IO", "outputDir", "./");

    if (access(outputDir.c_str(), F_OK & W_OK) < 0) {
      //若文件夹不存在则创建
      mkdir(outputDir.c_str(), 0751);
    }

    std::string mapName;
    for (int i = inputFile.size() - 1; i >= 0; i--) {
      if (inputFile[i] == '/') {
        mapName = inputFile.substr(i + 1, inputFile.size() - i - 5);
        break;
      }
    }
    // 存储时间矩阵
    std::string tMatFile = outputDir + mapName + "TimeMat_" + method + ".csv";
    // std::cout <<tMatFile;
    std::ofstream ofs(tMatFile, std::ios::out);
    for (int i = 0; i < gm.rows(); i++) {
      for (int j = 0; j < gm.cols(); j++) {
        GridCell *curCell = gm.getCellByRowCol(i, j);
        double t = curCell->arrivalTime();
        if (isInf(t)) {
          // 区分不可达点和障碍物
          if (isZero(curCell->velocity())) {
            // 障碍物
            ofs << "Inf";
          } else {
            // 不可达点
            ofs << "NaN";
          }

          // 不区分
          // ofs << "inf";
        } else {
          ofs << t;
        }
        if (j != gm.cols() - 1) {
          ofs << ", ";
        }
      }
      ofs << "\n";
    }
    ofs.clear();
    ofs.close();
    std::cout << "Time matrix saved in " << tMatFile << std::endl;

    // 保存路径
    if (shouldSolvePath == false) {
      return 0;
    }
    std::string pathFile = outputDir + mapName + "Path_" + method + ".csv";
    ofs.open(pathFile, std::ios::out);
    for (const auto &row_col : path) {
      ofs << row_col.first << ", " << row_col.second << "\n";
    }
    ofs.clear();
    ofs.close();
    std::cout << "Path saved in " << pathFile << std::endl;
    return 0;
  }
}
