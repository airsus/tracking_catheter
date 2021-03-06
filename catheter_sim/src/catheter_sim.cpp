#include <ros/ros.h> 
#include <geometry_msgs/Point.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Path.h>
#include <particle_filter/ParticleFilter.h>
#include <iostream>
#include <geometry_msgs/PoseArray.h>
#include <sensor_msgs/PointCloud.h>
#include <particle_filter/catheter_msg.h>
#include <std_msgs/Float32.h>

using namespace std;
using namespace ros;

#define MV 0.001 // the measurement variance is 0.001

double g_amps;
bool g_gotamps=false;
void CallBack(const std_msgs::Float32& amps)
{
    g_amps = amps.data;
    g_gotamps = true;
    ROS_INFO("Got input %f amps.", g_amps);
}


nav_msgs::Path getCatheterPath(CatheterPose Cp, int nm_segs);
sensor_msgs::PointCloud convertCatheter2Pc(CatheterPose cp);
particle_filter::catheter_msg setMeasuredCatheter(CatheterPose cp);
double sample(double b);

int main(int argc, char **argv) 
{ 
    ros::init(argc,argv,"catheter_sim"); 
    ros::NodeHandle n;
    srand(time(NULL));  
    /* Set one particle with small covariances as the ideal catheter*/
    ParticleFilter pf(1);
    geometry_msgs::Point v_B;
    double v_a; 
    v_B.x=0.0000001;
    v_B.y=0.0000001;
    v_B.z=0.0000001;
    v_a = 0.0000001;
    pf.setMotionErrors(v_B,v_a);
    std::vector<CatheterPose> Ct;
    Ct.resize(1);

    /* Set the catheter publisher*/
    ros::Publisher catheter_rviz_pub = n.advertise<nav_msgs::Path>("simulated_catheter_rviz", 1);
    ros::Publisher catheter_pts_pub = n.advertise<sensor_msgs::PointCloud>("catheter_pts_rviz", 1);
    ros::Publisher catheter_msrd_pub = n.advertise<particle_filter::catheter_msg>("measured_catheter", 1);
    ros::Publisher catheter_msrd_pub2 = n.advertise<sensor_msgs::PointCloud>("measured_catheter_rviz", 1);
    ros::Subscriber amps_sub = n.subscribe("amps",1,CallBack);

    /* Set the infomation about the catheter in rviz*/
    CatheterPose Cp;
    nav_msgs::Path Path;
    int nm_segs = 16; // N-1 segments in the catheter
    sensor_msgs::PointCloud cloud;
    sensor_msgs::PointCloud m_cloud;
    cloud.header.stamp = ros::Time::now();
    cloud.header.frame_id = "catheter_base";
    cloud.points.resize(3);
    particle_filter::catheter_msg M_Cp; // the measured Catheter, which contains errors
    CatheterPose m_cp; 

    /* Simulation*/
    while(!g_gotamps)
    {
        ros::spinOnce();
        ROS_INFO("wait for amps input...");
        ros::Duration(1).sleep();
    }
    int j=0;
    while(ros::ok())
    {
        ros::spinOnce();
        pf.generateParticles(g_amps);
        Ct=pf.getParticles();
        Cp=Ct[0];
        if(++j == 200)
        {
            j=0;
            Cp.displayCatheter();
        }
        Path = getCatheterPath(Cp, nm_segs);
        catheter_rviz_pub.publish(Path);

        cloud = convertCatheter2Pc(Cp);
        catheter_pts_pub.publish(cloud);

        M_Cp = setMeasuredCatheter(Cp);
        catheter_msrd_pub.publish(M_Cp);

        m_cp.A_.x = M_Cp.A.x;
        m_cp.A_.y = M_Cp.A.y;
        m_cp.A_.z = M_Cp.A.z;

        m_cp.B_.x = M_Cp.B.x;
        m_cp.B_.y = M_Cp.B.y;
        m_cp.B_.z = M_Cp.B.z;

        m_cp.C_.x = M_Cp.C.x;
        m_cp.C_.y = M_Cp.C.y;
        m_cp.C_.z = M_Cp.C.z;

        m_cloud = convertCatheter2Pc(m_cp);
        catheter_msrd_pub2.publish(m_cloud);

        ros::Duration(0.02).sleep();

    }

    return 0;
}

nav_msgs::Path getCatheterPath(CatheterPose Cp, int nm_segs)
{
    nav_msgs::Path Path;
    Path.header.frame_id = "catheter_base";
    Path.header.stamp = ros::Time::now();
    Path.poses.resize(nm_segs);

    geometry_msgs::Point Pt;
    geometry_msgs::PoseStamped Pose;

    Pose.header.frame_id = "catheter_base";
    Pose.header.stamp = ros::Time::now();

    double s=0;
    double step = 1.0/(nm_segs-1.0);
    for (int i=0; i<nm_segs; i++)
    {
        Pt = Cp.getBSpline(s);
        Pt.x = Pt.x*10;
        Pt.y = Pt.y*10;
        Pt.z = Pt.z*10;
        Pose.pose.position = Pt;
        Pose.header.seq = i;
        Path.poses[i] = Pose;

        s += step;
    }

    return Path;

}

sensor_msgs::PointCloud convertCatheter2Pc(CatheterPose cp)
{
    sensor_msgs::PointCloud cloud;
    cloud.header.stamp = ros::Time::now();
    cloud.header.frame_id = "catheter_base";
    cloud.points.resize(3);
    cloud.points[0].x = cp.A_.x*10;
    cloud.points[0].y = cp.A_.y*10;
    cloud.points[0].z = cp.A_.z*10;

    cloud.points[1].x = cp.B_.x*10;
    cloud.points[1].y = cp.B_.y*10;
    cloud.points[1].z = cp.B_.z*10;

    cloud.points[2].x = cp.C_.x*10;
    cloud.points[2].y = cp.C_.y*10;
    cloud.points[2].z = cp.C_.z*10;

    return cloud;
}

particle_filter::catheter_msg setMeasuredCatheter(CatheterPose cp)
{
    particle_filter::catheter_msg mcp;
    mcp.A.x = cp.A_.x + sample(MV);
    mcp.A.y = cp.A_.y + sample(MV);
    mcp.A.z = cp.A_.z + sample(MV);

    mcp.B.x = cp.B_.x + sample(MV);
    mcp.B.y = cp.B_.y + sample(MV);
    mcp.B.z = cp.B_.z + sample(MV);

    mcp.C.x = cp.C_.x + sample(MV);
    mcp.C.y = cp.C_.y + sample(MV);
    mcp.C.z = cp.C_.z + sample(MV);

    return mcp;
}

double sample(double b)
{
    double x=0, r=0;
    for(int j=0; j < 12; j++)
    {
        r=(rand()*2.0/RAND_MAX-1.0)*b;
        x=x+r;
    }
    return x/2;
}
