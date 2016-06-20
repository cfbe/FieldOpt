#include <gtest/gtest.h>
#include <QList>
#include "Reservoir/grid/grid.h"
#include "Reservoir/grid/eclgrid.h"
#include "Reservoir/grid/grid_exceptions.h"
#include "WellIndexCalculator/geometry_functions/geometryfunctions.h"
#include "Utilities/file_handling/filehandling.h"
#include "Reservoir/well_index_calculation/wellindexcalculator.h"

using namespace Reservoir::Grid;
using namespace Reservoir::WellIndexCalculation;

namespace {

    class SingleCellWellIndexTest : public ::testing::Test {
    protected:
        SingleCellWellIndexTest() {
            grid_ = new ECLGrid(file_path_);
        }

        virtual ~SingleCellWellIndexTest() {
            delete grid_;
        }

        virtual void SetUp() {
        }

        virtual void TearDown() { }


        Grid *grid_;
        QString file_path_ = "../examples/ADGPRS/5spot/ECL_5SPOT.EGRID";
    };

    TEST_F(SingleCellWellIndexTest, WellIndexValueWithQVector_test) {

        // Load grid and chose first icell (icell 1,1,1)
        //double kx = 1.689380;
        //double ky = 1.689380;
        //double kz = 1;
        //Figure out conversions and shit?
        double wellbore_radius = 0.1905/2;

        auto cell_1 = grid_->GetCell(0);
        auto ptr_cell_1 = cell_1;
        auto corners = cell_1.corners();

        //Determine well placement. Let it go vertically through the centre of the block.
        double well_start_x = 0.25*corners[0].x() + 0.25*corners[1].x() +0.25*corners[2].x() + 0.25*corners[3].x();
        double well_start_y = 0.25*corners[0].y() + 0.25*corners[1].y() +0.25*corners[2].y() + 0.25*corners[3].y();
        double well_start_z = 0.25*corners[0].z() + 0.25*corners[1].z() +0.25*corners[2].z() + 0.25*corners[3].z();

        double well_end_x = 0.25*corners[4].x() + 0.25*corners[5].x() +0.25*corners[6].x() + 0.25*corners[7].x();
        double well_end_y = 0.25*corners[4].y() + 0.25*corners[5].y() +0.25*corners[6].y() + 0.25*corners[7].y();
        double well_end_z = 0.25*corners[4].z() + 0.25*corners[5].z() +0.25*corners[6].z() + 0.25*corners[7].z();
        Eigen::Vector3d start_point = Eigen::Vector3d(well_start_x,well_start_y,well_start_z);
        Eigen::Vector3d end_point = Eigen::Vector3d(well_end_x,well_end_y, well_end_z);
        auto icell = IntersectedCell(cell_1);
        icell.set_entry_point(start_point);
        icell.set_exit_point(end_point);
        double wi = WellIndexCalculator::well_index_cell(icell, wellbore_radius);
        /* 0.555602 is the expected well transmisibility factor aka. well index.
         * For now this value is read directly from eclipse output file:
         * Expect value within delta percent
         */
        double delta = 0.001;
        double delta_percent =1+(delta/100);
        EXPECT_TRUE( wi < 0.555602*(delta_percent));
        EXPECT_TRUE( wi > 0.555602/(delta_percent));
    }

    TEST_F(SingleCellWellIndexTest, vertical_well_index_test) {
        double wellbore_radius = 0.1905/2;

        auto cell_1 = grid_->GetCell(0);
        QList<Eigen::Vector3d> corners = cell_1.corners();

        //Determine well placement. Let it go vertically through the centre of the block.
        double well_start_x = 0.25*corners[0].x() + 0.25*corners[1].x() +0.25*corners[2].x() + 0.25*corners[3].x();
        double well_start_y = 0.25*corners[0].y() + 0.25*corners[1].y() +0.25*corners[2].y() + 0.25*corners[3].y();
        double well_start_z = 0.25*corners[0].z() + 0.25*corners[1].z() +0.25*corners[2].z() + 0.25*corners[3].z();

        double well_end_x = 0.25*corners[4].x() + 0.25*corners[5].x() +0.25*corners[6].x() + 0.25*corners[7].x();
        double well_end_y = 0.25*corners[4].y() + 0.25*corners[5].y() +0.25*corners[6].y() + 0.25*corners[7].y();
        double well_end_z = 0.25*corners[4].z() + 0.25*corners[5].z() +0.25*corners[6].z() + 0.25*corners[7].z();
        Eigen::Vector3d start_point = Eigen::Vector3d(well_start_x, well_start_y, well_start_z);
        Eigen::Vector3d end_point= Eigen::Vector3d(well_end_x,well_end_y, well_end_z);
        Reservoir::WellIndexCalculation::IntersectedCell icell(cell_1);
        icell.set_entry_point(start_point);
        icell.set_exit_point(end_point);

        double wi = WellIndexCalculator::well_index_cell(icell, wellbore_radius);
        // WellIndexCalculation::GeometryFunctions::vertical_well_index_cell(cell_1,kx,ky,wellbore_radius);
        /* 0.555602 is the expected well transmisibility factor aka. well index.
         * For now this value is read directly from eclipse output file:
         * Expect value within delta percent
         */
        double delta = 0.0001;
        double delta_percent =1+(delta/100);
        EXPECT_TRUE( wi < 0.555602*(delta_percent));
        EXPECT_TRUE( wi > 0.555602/(delta_percent));
    }

    TEST_F(SingleCellWellIndexTest, Well_index_grid_test) {
        double wellbore_radius = 0.191/2;

        Eigen::Vector3d start_point = Eigen::Vector3d(0.05,0.00,1712);
        Eigen::Vector3d end_point= Eigen::Vector3d(1440.0,1400.0,1712);
        QList<Eigen::Vector3d> well_spline_points = {start_point, end_point};

        // \todo The following lines need to be changed
        auto wic = WellIndexCalculator(grid_);
        auto blocks = wic.ComputeWellBlocks(start_point, end_point, wellbore_radius);

        auto pair = wic.compute_well_indices();
        EXPECT_EQ(118, pair.length());
/*
    std::ofstream myfile;
    myfile.open ("test_44.txt");
    myfile << "well starting point (x,y,z)= " << start_point.x() <<"," << start_point.y() << "," << start_point.z() << ")\n";
    myfile << "well ending point (x,y,z)= " << end_point.x() <<"," << end_point.y() << "," << end_point.z() << ")\n";
    myfile << "number of cells intersected = " << pair.first.length() << "\n";
    myfile << "number of well index values = " << pair.second.length() << "\n";
    for( int ii = 0; ii<pair.first.length(); ii++){
        myfile << "cell number " << pair.first.at(ii) << " with permeability kx = " << grid_->GetCell(pair.first.at(ii)).permx() <<", ky = "<< grid_->GetCell(pair.first.at(ii)).permy()<< ", kz = "<< grid_->GetCell(pair.first.at(ii)).permz()<<" has well index = " << pair.second.at(ii) <<"\n";
    }
        myfile.close();
    WellIndexCalculation::GeometryFunctions::print_well_index_file(grid_,well_spline_points, end_points, wellbore_radius, 0.00001, "NewlyTried1422");
*/
        EXPECT_TRUE(true);
    }

}


