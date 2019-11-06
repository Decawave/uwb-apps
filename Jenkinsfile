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
            steps {
                echo 'Building..'
                sh 'rm -rf ${JENKINS_HOME}/ci;git clone https://github.com/decawave/mynewt-travis-ci ${JENKINS_HOME}/ci'
                sh 'chmod +x ${JENKINS_HOME}/ci/jenkins/*.sh'
                sh '${JENKINS_HOME}/ci/jenkins/linux_jenkins_install.sh'
                sh 'cp -r ${JENKINS_HOME}/ci/uwb-apps-project.yml project.yml'
                sh 'cp -r ${JENKINS_HOME}/ci/uwb-apps-targets targets'
                sh 'newt upgrade'
                sh '${JENKINS_HOME}/ci/jenkins/uwb-apps-setup.sh'
            }
        }
        stage('Build') {
            steps {
                echo 'Building..'
                sh '${JENKINS_HOME}/ci/jenkins/prepare_test.sh ${TOTAL_SETS}'
                sh '${JENKINS_HOME}/ci/jenkins/run_test.sh'
            }
        }
        stage('Test') {
            steps {
                echo 'Testing....'
            }
        }
    }
}
