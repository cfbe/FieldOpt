/***********************************************************
 Copyright (C) 2015-2017
 Einar J.M. Baumann <einar.baumann@gmail.com>

 This file is part of the FieldOpt project.

 FieldOpt is free software: you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation, either version
 3 of the License, or (at your option) any later version.

 FieldOpt is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License for more details.

 You should have received a copy of the GNU
 General Public License along with FieldOpt.
 If not, see <http://www.gnu.org/licenses/>.
***********************************************************/

// ---------------------------------------------------------
#include "model.h"
#include <boost/lexical_cast.hpp>
#include <wells/well_group.h>

// ---------------------------------------------------------
using std::cout;
using std::endl;

// ---------------------------------------------------------
namespace Model {

// =========================================================
Model::Model(Settings::Model* settings, Logger *logger) {

  // -------------------------------------------------------
  settings_ = settings;

  // -------------------------------------------------------
  if (settings_->verb_vector()[5] > 1) { // idx:5 -> mod
    cout << BRED << "[mod]Building Model.---------" << AEND << endl;
    cout << "[mod]Init ECLGrid_.---------- " << endl;
  }

  // -------------------------------------------------------
  grid_ = new Reservoir::Grid::ECLGrid(
      settings_->reservoir().path.toStdString());

  // -------------------------------------------------------
  if (settings_->verb_vector()[5] > 1) // idx:5 -> mod
    cout << "[mod]Init var container.-----" << endl;
  variable_container_ = new Properties::VariablePropertyContainer();

  // -------------------------------------------------------
  // if (settings_->verb_vector()[5] >= 1) // idx:5 -> mod
  //  cout << "[mod]Adding wells:----------- \n";

  // -------------------------------------------------------
  SetDrillingSeq(); // Establishes all fields in drillseq_
  GetDrillingStr(); // Dbg

  if(drillseq_->drill_groups_.size() > 1) {

  // -------------------------------------------------------
    CreateWellGroups();

  } else {
    // -----------------------------------------------------
    CreateWells();
  }

  // -------------------------------------------------------
  variable_container_->CheckVariableNameUniqueness();
  logger_ = logger;
  logger_->AddEntry(new Summary(this));

  // -------------------------------------------------------
  if (settings_->verb_vector()[5] > 1) // idx:5 -> mod (Model)
    cout << BRED << "[mod]Finished building Model." << AEND << endl;

}

// =========================================================
void Model::CreateWellGroups() {

  // -------------------------------------------------------
  // Empty QList to contain groups of wells
  well_groups_ = new QList<WellGroups::WellGroup *>();

  // -------------------------------------------------------
  for (int gnr = 0; gnr < drillseq_->drill_groups_.size(); ++gnr) {
    well_groups_->append(
        new WellGroups::WellGroup(*settings_,
                                  gnr,
                                  *drillseq_,
                                  variable_container_,
                                  grid_));
  }
}

// =========================================================
void Model::CreateWells() {

  // -------------------------------------------------------
  // Create wells
  wells_ = new QList<Wells::Well *>();
  for (int wnr = 0; wnr < settings_->wells().size(); ++wnr) {

    // -----------------------------------------------------
    auto wname = settings_->wells().at(wnr).name.toStdString();
    if (settings_->verb_vector()[5] >= 1) // idx:5 -> mod (Model)
      cout << FCYAN << "\nWell=" << wname
           << "\n----------------------------- \n" << AEND;

    // -----------------------------------------------------
    wells_->append(new Wells::Well(*settings_,
                                   wnr,
                                   variable_container_,
                                   grid_));
  }

}

// =========================================================
Wells::Well* Model::getWell(QString well_name) {

  // -------------------------------------------------------
  for (int wnr = 0; wnr < wells_->size(); ++wnr) {
    if(wells_->at(wnr)->name().compare(well_name)) {
      return wells_->at(wnr);
    }
  }
}

// =========================================================
void Model::Finalize() {

  logger_->AddEntry(this); // Removing this causes
  // the last case to not be in the JSON file
  logger_->AddEntry(new Summary(this));
}

// =========================================================
void Model::ApplyCase(Optimization::Case *c, int rank) {

  // -------------------------------------------------------
  for (QUuid key : c->binary_variables().keys()) {
    variable_container_->SetBinaryVariableValue(
        key, c->binary_variables()[key]);
  }

  // -------------------------------------------------------
  for (QUuid key : c->integer_variables().keys()) {
    variable_container_->SetDiscreteVariableValue(
        key, c->integer_variables()[key]);
  }

  // -------------------------------------------------------
  for (QUuid key : c->real_variables().keys()) {
    variable_container_->SetContinousVariableValue(
        key, c->real_variables()[key]);
  }

  // -------------------------------------------------------
  int cumulative_wic_time = 0;
  for (Wells::Well *w : *wells_) {
    w->Update(rank);
    cumulative_wic_time += w->GetTimeSpentInWIC();
  }

  // -------------------------------------------------------
  c->SetWICTime(cumulative_wic_time);
  verify();

  // -------------------------------------------------------
  // Notify the logger, and after that clear the results.
  // First check that we have results (if not, this is the
  // first evaluation, and we have nothing to notify the
  // logger about).
  if (results_.size() > 0){
    logger_->AddEntry(this);
  }

  // -------------------------------------------------------
  current_case_id_ = c->id();
  results_.clear();
}

// =========================================================
void Model::verify() {
  verifyWells();
}

// =========================================================
void Model::verifyWells() {
  for (Wells::Well *well : *wells_) {
    verifyWellTrajectory(well);
  }
}

// =========================================================
void Model::verifyWellTrajectory(Wells::Well *w) {

  for (Wells::Wellbore::WellBlock *wb : *w->trajectory()->GetWellBlocks()) {
    verifyWellBlock(wb);
  }
}

// =========================================================
void Model::verifyWellBlock(Wells::Wellbore::WellBlock *wb) {

  if (wb->i() < 1 || wb->i() > grid()->Dimensions().nx ||
      wb->j() < 1 || wb->j() > grid()->Dimensions().ny ||
      wb->k() < 1 || wb->k() > grid()->Dimensions().nz)
    throw std::runtime_error(
        "Invalid well block detected: ("
            + boost::lexical_cast<std::string>(wb->i()) + ", "
            + boost::lexical_cast<std::string>(wb->j()) + ", "
            + boost::lexical_cast<std::string>(wb->k()) + ")"
    );
}

// =========================================================
void Model::SetResult(const std::string key,
                      std::vector<double> vec) {
  results_[key] = vec;
}

// =========================================================
Loggable::LogTarget Model::GetLogTarget() {
  return Loggable::LogTarget::LOG_EXTENDED;
}

// =========================================================
map<string, string> Model::GetState() {
  map<string, string> statemap;
  statemap["COMPDAT"] = compdat_.toStdString();
  return statemap;
}

// =========================================================
QUuid Model::GetId() {
  return current_case_id_;
}

// =========================================================
map<string, vector<double>> Model::GetValues() {

  // -------------------------------------------------------
  map<string, vector<double>> valmap;
  for (auto const item : results_) {
    valmap["Res#"+item.first] = item.second;
  }

  // -------------------------------------------------------
  for (auto const var :
      variable_container_->GetContinousVariables()->values()) {
    valmap["Var#"+var->name().toStdString()] = vector<double>{var->value()};
  }

  // -------------------------------------------------------
  for (auto const var :
      variable_container_->GetDiscreteVariables()->values()) {
    valmap["Var#"+var->name().toStdString()] = vector<double>{var->value()};
  }

  // -------------------------------------------------------
  for (auto const var :
      variable_container_->GetBinaryVariables()->values()) {
    valmap["Var#"+var->name().toStdString()] = vector<double>{var->value()};
  }

  return valmap;
}

// =========================================================
Loggable::LogTarget Model::Summary::GetLogTarget() {
  return LOG_SUMMARY;
}

// =========================================================
map<string, string> Model::Summary::GetState() {
  map<string, string> statemap;
  statemap["compdat"] = model_->compdat_.toStdString();
  return statemap;
}

// =========================================================
QUuid Model::Summary::GetId() {
  return nullptr;
}

// =========================================================
map<string, vector<double>> Model::Summary::GetValues() {
  map<string, vector<double>> valmap;
  return valmap;
}

// =========================================================
map<string, Loggable::WellDescription>
Model::Summary::GetWellDescriptions() {

  // -------------------------------------------------------
  map<string, Loggable::WellDescription> wellmap;

  // -------------------------------------------------------
  for (auto well : *model_->wells()) {

    // -----------------------------------------------------
    Loggable::WellDescription wdesc;
    wdesc.name = well->name().toStdString();
    wdesc.group = well->group().toStdString();
    wdesc.wellbore_radius = boost::lexical_cast<string>(well->wellbore_radius());
    wdesc.type = well->IsProducer() ? "Producer" : "Injector";

    // -----------------------------------------------------
    switch (well->preferred_phase()) {
      case Settings::Model::PreferredPhase::Oil:
        wdesc.pref_phase = "Oil"; break;

      case Settings::Model::PreferredPhase::Gas:
        wdesc.pref_phase = "Gas"; break;

      case Settings::Model::PreferredPhase::Water:
        wdesc.pref_phase = "Water"; break;

      case Settings::Model::PreferredPhase::Liquid:
        wdesc.pref_phase = "Liquid"; break;
    }

    // -----------------------------------------------------
    // Spline
    if (model_->variables()->
        GetWellSplineVariables(well->name()).size() > 0) {
      wdesc.def_type = "Spline";

      // ---------------------------------------------------
      for (auto prop :
          model_->variables()->
              GetWellSplineVariables(well->name())) {

        // -------------------------------------------------
        if (prop->propertyInfo().spline_end ==
            Properties::Property::SplineEnd::Heel) {

          switch (prop->propertyInfo().coord) {
            case Properties::Property::Coordinate::x:
              wdesc.spline.heel_x = prop->value(); break;

            case Properties::Property::Coordinate::y:
              wdesc.spline.heel_y = prop->value(); break;

            case Properties::Property::Coordinate::z:
              wdesc.spline.heel_z = prop->value(); break;
          }
        } else {
          switch (prop->propertyInfo().coord) {
            case Properties::Property::Coordinate::x:
              wdesc.spline.toe_x = prop->value(); break;

            case Properties::Property::Coordinate::y:
              wdesc.spline.toe_y = prop->value(); break;

            case Properties::Property::Coordinate::z:
              wdesc.spline.toe_z = prop->value(); break;
          }
        }
      }

    } else {
      wdesc.def_type =  "Blocks";
    }

    // -----------------------------------------------------
    // Controls
    for (Wells::Control *cont : *well->controls()) {
      Loggable::ControlDescription cd;
      if (cont->mode() == Settings::Model::ControlMode::RateControl) {
        cd.control = "Rate";
        cd.value = cont->rate();
      }
      else {
        cd.control = "BHP";
        cd.value = cont->bhp();
      }
      cd.state = cont->open() ? "Open" : "Shut";
      cd.time_step = cont->time_step();
      wdesc.controls.push_back(cd);
    }
    wellmap[well->name().toStdString()] = wdesc;
  }

  return wellmap;
}

// =========================================================
void Model::GetDrillingStr() {

  // ---------------------------------------------------------
  for(int i=0; i < drillseq_->drill_groups_.size(); ++i) {

    cout << "Group: [ "
         << drillseq_->drill_groups_[i] << " ]" << endl;

  }

  // ---------------------------------------------------------
  auto seq_vec_group = drillseq_->wseq_grpd_sorted_name;
  for(int i=0; i < seq_vec_group.size(); ++i) {

    for (int j=0; j<seq_vec_group[i].size(); ++j) {

      cout << "IntSeq: [ "
           << seq_vec_group[i][j].first << " -- "
           << seq_vec_group[i][j].second << " ]"
           << endl;

    }

  }

  // ---------------------------------------------------------
  auto seq_vec_time = drillseq_->wseq_grpd_sorted_vs_time;
  for(int i=0; i < seq_vec_time.size(); ++i) {

    cout << "TimeSeq: [ "
         << seq_vec_time[i].first << " -- "
         << seq_vec_time[i].second << " ]"
         << endl;

  }

}

// =========================================================
void Model::SetDrillingSeq() {

  // -------------------------------------------------------
  if (settings_->verb_vector()[5] >= 1) // idx:5 -> mod
    cout << "\n[mod]Compute drilling seq.--- \n";
  drillseq_->mode = settings_->drillingMode_;

  // -------------------------------------------------------
  // Main drilling name_vs_order maps (pairs)
  for (int i = 0; i < wells_->size(); ++i) {

    // -----------------------------------------------------
    pair<string, pair<int, int>>
        well_order(wells_->at(i)->name().toStdString(),
                   wells_->at(i)->GetDrillingOrder());
    drillseq_->name_vs_order.push_back(well_order);

    // -----------------------------------------------------
    drillseq_->name_vs_time.emplace(wells_->at(i)->name().toStdString(),
                           wells_->at(i)->GetDrillingTime());

    // -----------------------------------------------------
    drillseq_->name_vs_num
    [wells_->at(i)->name().toStdString()] = i;
  }

  // -------------------------------------------------------
  // Use multimap to group well pairs (<int, pair<int, string>>)
  // into groups
  std::set<int> groups;
  for (int i = 0; i < drillseq_->name_vs_order.size(); ++i) {

    // -----------------------------------------------------
    pair<int, string> p(drillseq_->name_vs_order[i].second.second,
                        drillseq_->name_vs_order[i].first);

    // -----------------------------------------------------
    drillseq_->mp_wells_into_groups.emplace(
        drillseq_->name_vs_order[i].second.first, p);

    // -----------------------------------------------------
    groups.emplace(drillseq_->name_vs_order[i].second.first);

  }

  // -------------------------------------------------------
  drillseq_->drill_groups_ =
      std::vector<int> (groups.begin(), groups.end());

  // -------------------------------------------------------
  // Order each multimap group into a (pair<int, string>)
  // vector with wells within each group sorted
  decltype(drillseq_->mp_wells_into_groups.equal_range(int())) range;

  // -----------------------------------------------------
  // Find equal ranges in multimap
  for (auto i = drillseq_->mp_wells_into_groups.begin();
       i != drillseq_->mp_wells_into_groups.end(); i = range.second) {

    range = drillseq_->mp_wells_into_groups.equal_range(i->first);

    // ---------------------------------------------------
    // Insert each well in group into vector
    vector<pair<int, string>> group_vec;
    for(auto d = range.first; d != range.second; ++d) {

      // -------------------------------------------------
      // Assemble well pair
      pair<int, string> well_pair(d->second.first,
                                  d->second.second);
      group_vec.push_back(well_pair);
    }

    // ---------------------------------------------------
    // Sort group vector
    sort(group_vec.begin(), group_vec.end());

    // ---------------------------------------------------
    drillseq_->wseq_grpd_sorted_name.push_back(group_vec);
  }

  // -------------------------------------------------------
  for( int i=0; i < drillseq_->wseq_grpd_sorted_name.size(); ++i ) {

    for (int j=0; j< drillseq_->wseq_grpd_sorted_name[i].size(); j++) {

      // ---------------------------------------------------
      string wn = drillseq_->wseq_grpd_sorted_name[i][j].second;
      pair<string, double>
          p(wn,
            drillseq_->name_vs_time.find(wn)->second);

      // ---------------------------------------------------
      drillseq_->wseq_grpd_sorted_vs_time.push_back(p);

    }

  }

}
}
