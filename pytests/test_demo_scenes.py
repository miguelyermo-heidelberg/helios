import os
import shutil
import subprocess
import sys
from pathlib import Path
import pytest

MAX_DIFFERENCE_BYTES = 1024
DELETE_FILES_AFTER = False
HELIOS_EXE = str(Path('run') / 'helios')
if sys.platform == "win32":
    HELIOS_EXE += ".exe"
WORKING_DIR = str(Path(__file__).parent.parent.absolute())


def find_playback_dir(survey_path):
    playback = Path(WORKING_DIR) / 'output'
    with open(Path(WORKING_DIR) / survey_path, 'r') as sf:
        for line in sf:
            if '<survey name' in line:
                survey_name = line.split('name="')[1].split('"')[0]
    if not (playback / survey_name).is_dir():
        raise Exception('Could not locate output directory')
    last_run_dir = sorted(list((playback / survey_name).glob('*')), key=lambda f: f.stat().st_ctime, reverse=True)[0]
    return last_run_dir


def run_helios_executable(survey_path: Path, options=None) -> Path:
    if options is None:
        options = list()
    command = [HELIOS_EXE, str(survey_path)] + options + ['--rebuildScene', '--seed', '43', '-vt', '-j', '1']
    print(command)
    # shell must be false for linux (but true for windows(?))
    p = subprocess.Popen(command, cwd=WORKING_DIR, shell=(sys.platform == "win32"))
    p.wait()
    assert p.returncode == 0
    return find_playback_dir(survey_path)


def run_helios_pyhelios(survey_path: Path, zip_output=False) -> Path:
    sys.path.append(WORKING_DIR)
    import pyhelios
    pyhelios.setDefaultRandomnessGeneratorSeed("43")
    from pyhelios import SimulationBuilder
    simB = SimulationBuilder(
        surveyPath=str(survey_path.absolute()),
        assetsDir=WORKING_DIR + os.sep + 'assets' + os.sep,
        outputDir=WORKING_DIR + os.sep + 'output' + os.sep,
    )
    simB.setLasOutput(True)
    simB.setRebuildScene(True)
    simB.setZipOutput(zip_output)
    simB.setNumThreads(1)
    simB.setKDTJobs(1)

    sim = simB.build()

    sim.start()
    sim.join()
    return find_playback_dir(survey_path)


@pytest.mark.exe
def test_arbaro_tls_exe():
    dirname_exe = run_helios_executable(Path('data') / 'surveys' / 'demo' / 'tls_arbaro_demo.xml',
                                        options=['--lasOutput'])
    eval_arbaro_tls(dirname_exe)


@pytest.mark.pyh
def test_arbaro_tls_pyh():
    dirname_pyh = run_helios_pyhelios(Path('data') / 'surveys' / 'demo' / 'tls_arbaro_demo.xml')
    eval_arbaro_tls(dirname_pyh)


def eval_arbaro_tls(dirname):
    assert (dirname / 'leg000_points.las').exists()
    assert abs((dirname / 'leg000_points.las').stat().st_size - 15_218_537) < MAX_DIFFERENCE_BYTES
    assert (dirname / 'leg001_points.las').exists()
    assert abs((dirname / 'leg001_points.las').stat().st_size - 9_773_929) < MAX_DIFFERENCE_BYTES
    with open(dirname / 'leg000_trajectory.txt', 'r') as f:
        line = f.readline()
        assert line.startswith('1.0000 25.5000 0.0000')
    # clean up
    if DELETE_FILES_AFTER:
        shutil.rmtree(dirname)


@pytest.mark.exe
def test_tiffloader_als_exe():
    dirname_exe = run_helios_executable(Path('data') / 'test' / 'als_hd_demo_tiff_min.xml',
                                        options=['--lasOutput'])
    eval_tiffloader_als(dirname_exe)


@pytest.mark.pyh
def test_tiffloader_als_pyh():
    dirname_pyh = run_helios_pyhelios(Path('data') / 'test' / 'als_hd_demo_tiff_min.xml')
    eval_tiffloader_als(dirname_pyh)


def eval_tiffloader_als(dirname):
    assert (dirname / 'leg000_points.las').exists()
    assert abs((dirname / 'leg000_points.las').stat().st_size - 105_049) < MAX_DIFFERENCE_BYTES
    assert (dirname / 'leg001_points.las').exists()
    assert abs((dirname / 'leg001_points.las').stat().st_size - 105_049) < MAX_DIFFERENCE_BYTES
    with open(dirname / 'leg000_trajectory.txt', 'r') as f:
        next(f)
        line = f.readline()
        assert line.startswith('474500.7510 5474500.0000 1500.0000')
    # clean up
    if DELETE_FILES_AFTER:
        shutil.rmtree(dirname)


@pytest.mark.exe
def test_detailedVoxels_uls_exe():
    dirname_exe = run_helios_executable(Path('data') / 'test' / 'uls_detailedVoxels_mode_comparison_min.xml',
                                        options=['--lasOutput'])
    eval_detailedVoxels_uls(dirname_exe)


@pytest.mark.pyh
def test_detailedVoxels_uls_pyh():
    dirname_pyh = run_helios_pyhelios(Path('data') / 'test' / 'uls_detailedVoxels_mode_comparison_min.xml')
    eval_detailedVoxels_uls(dirname_pyh)


def eval_detailedVoxels_uls(dirname):
    assert (dirname / 'leg000_points.las').exists()
    assert abs((dirname / 'leg000_points.las').stat().st_size - 412_109) < MAX_DIFFERENCE_BYTES
    assert (dirname / 'leg000_trajectory.txt').exists()
    assert abs((dirname / 'leg000_trajectory.txt').stat().st_size - 1_250) < MAX_DIFFERENCE_BYTES
    with open(dirname / 'leg000_trajectory.txt', 'r') as f:
        for _ in range(6):
            next(f)
        line = f.readline()
        assert line.startswith('-3.0000 -1.2741 50.0000')
    # clean up
    if DELETE_FILES_AFTER:
        shutil.rmtree(dirname)


@pytest.mark.exe
def test_xyzVoxels_tls_exe():
    dirname_exe = run_helios_executable(Path('data') / 'surveys' / 'voxels' / 'tls_sphere_xyzloader_normals.xml',
                                        options=['--lasOutput'])
    eval_xyzVoxels_tls(dirname_exe)


@pytest.mark.pyh
def test_xyzVoxels_tls_pyh():
    dirname_pyh = run_helios_pyhelios(Path('data') / 'surveys' / 'voxels' / 'tls_sphere_xyzloader_normals.xml')
    eval_xyzVoxels_tls(dirname_pyh)


def eval_xyzVoxels_tls(dirname):
    assert (dirname / 'leg000_points.las').exists()
    assert abs((dirname / 'leg000_points.las').stat().st_size - 13_555_681) < MAX_DIFFERENCE_BYTES
    # clean up
    if DELETE_FILES_AFTER:
        shutil.rmtree(dirname)


@pytest.mark.exe
def test_interpolated_traj_exe():
    dirname_exe = run_helios_executable(Path('data') / 'surveys' / 'demo' / 'als_interpolated_trajectory.xml',
                                        options=['--lasOutput', '--zipOutput'])
    eval_interpolated_traj(dirname_exe)


@pytest.mark.pyh
def test_interpolated_traj_pyh():
    dirname_pyh = run_helios_pyhelios(Path('data') / 'surveys' / 'demo' / 'als_interpolated_trajectory.xml',
                                      zip_output=True)
    eval_interpolated_traj(dirname_pyh)


def eval_interpolated_traj(dirname):
    assert (dirname / 'leg000_points.laz').exists()
    assert (dirname / 'leg000_trajectory.txt').exists()
    assert abs((dirname / 'leg000_points.laz').stat().st_size - 153_538) < MAX_DIFFERENCE_BYTES
    with open(dirname / 'leg000_trajectory.txt', 'r') as f:
        for _ in range(3):
            next(f)
        line = f.readline()
        assert line.startswith('13.4766 1.7424 400.0000')
    # clean up
    if DELETE_FILES_AFTER:
        shutil.rmtree(dirname)
