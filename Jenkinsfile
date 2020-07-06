pipeline {
    agent {
        label 'ubuntu'
    }
    environment {
        TEST = 'BUILD_TARGETS'
        TOTAL_SETS = 1
        TARGET_SET = 1
    }
    stages {
        stage('Install') {
            environment {
                JENKINS_CI = "${env.WORKSPACE}/.ci"
                JENKINS_BIN = "${env.WORKSPACE}/../bin"
                TOOLCHAIN_PATH = "${env.WORKSPACE}/../gcc_toolchain"
            }
            steps {
                echo 'Installing...'
                sh 'rm -rf ${JENKINS_CI};git clone https://github.com/decawave/mynewt-travis-ci ${JENKINS_CI}'
                sh 'chmod +x ${JENKINS_CI}/jenkins/*.sh'
                sh '${JENKINS_CI}/jenkins/linux_jenkins_install.sh'
                sh '#cp -r ${JENKINS_CI}/jenkins/uwb-apps-project.yml project.yml'
                sh 'mkdir -p targets;cp -r ${JENKINS_CI}/uwb-apps-targets/* targets/'
                echo 'Remove any patches to mynewt-core if there..'
                sh '[ -d repos/apache-mynewt-core ] && (cd repos/apache-mynewt-core;git checkout -- ./;cd ${WORKSPACE}) || echo "nothing to do"'
                sh '''
                    if ! newt upgrade;then
                        echo "Need to remove mcuboot/ext/mbedtls due to bug in git"
                        rm -rf repos/mcuboot/ext/mbedtls;
                        newt upgrade;
                    fi
                '''
                echo 'Test for dw3000 access'
                sh 'bash repos/decawave-uwb-core/setup.sh'
                sh '${JENKINS_CI}/jenkins/uwb-apps-setup.sh'
            }
        }
        stage('Build') {
            environment {
                JENKINS_CI = "${env.WORKSPACE}/.ci"
                JENKINS_BIN = "${env.WORKSPACE}/../bin"
                TOOLCHAIN_PATH = "${env.WORKSPACE}/../gcc_toolchain"
            }
            steps {
                echo 'Building..'
                sh '${JENKINS_CI}/jenkins/prepare_test.sh ${TOTAL_SETS}'
                sh '${JENKINS_CI}/jenkins/run_test.sh'
            }
        }
        stage('Test') {
            environment {
                JENKINS_CI = "${env.WORKSPACE}/.ci"
                JENKINS_BIN = "${env.WORKSPACE}/../bin"
                TOOLCHAIN_PATH = "${env.WORKSPACE}/../gcc_toolchain"
            }
            steps {
                echo 'Testing....'
                sh '${JENKINS_CI}/jenkins/post_build.sh'
            }
        }
    }
}
