// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   ui/task/project_view.h
/// @brief  Project view class for ui library.
/// @author Sergey Lyskov (sergey.lyskov@jhu.edu).

#ifndef PROJECT_VIEW_H
#define PROJECT_VIEW_H

#include <ui/task/project_model.h>

#include <QMainWindow>

namespace Ui {
class ProjectView;
}

namespace ui {
namespace task {


class ProjectView : public QMainWindow
{
    Q_OBJECT

public:
    explicit ProjectView(QWidget *parent = 0);
    ~ProjectView();

public  Q_SLOTS:

private Q_SLOTS:
	void on_new_task_clicked();
    void on_delete_task_clicked();

    void on_action_new_project_triggered();
    void on_action_open_project_triggered();
    void on_action_save_project_triggered();
	void on_action_save_project_as_triggered();

	void on_action_preferences_triggered();

    void on_project_doubleClicked(const QModelIndex &index);

private:

	void save_project(bool avoid_asking_file_name_if_possible);


    Ui::ProjectView *ui;

	ProjectModel * project_model_ = nullptr;

	ProjectSP project_;
	QString file_name_;
};

} // namespace task
} // namespace ui


#endif // PROJECT_VIEW_H
